/**
 * ops/set_ui.ts — pure-TS port of MPFR's `mpfr_set_ui`.
 *
 * Convert a machine unsigned integer to an {@link MPFR} value at the
 * caller-supplied precision, rounded per the rounding mode, returning
 * the canonical `{value, ternary}` shape.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_ui(mpfr_t rop, unsigned long int op, mpfr_rnd_t rnd);
 *
 * The C implementation (mpfr/src/set_ui.c L25–L29) trivially delegates
 * to `mpfr_set_ui_2exp(rop, i, 0, rnd_mode)`, whose body lives in
 * mpfr/src/set_ui_2exp.c L26–L90. Structure is identical to set_si
 * except (a) the input is always non-negative, (b) the sign assignment
 * is `MPFR_SET_POS(x)` unconditional, (c) `mpfr_round_raw`'s `signb`
 * argument is `0` (since `i < 0` is impossible for an unsigned).
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_ui(n: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `n` as a non-negative `bigint` in `[0, ULONG_MAX]` —
 *     `[0, 2^64 - 1]`. Same rationale as set_si.ts for the `bigint`
 *     choice: JS `number` cannot losslessly hold values above
 *     `2^53 - 1`, well short of ULONG_MAX.
 *   - returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Range
 * -----
 *
 * `n` must lie in `[0, 2^64 - 1]`. Out-of-range (negative or above
 * ULONG_MAX) throws `MPFRError('EPREC', ...)`. The discriminant
 * rationale matches set_si.ts — EPREC is the closest fit in the
 * locked schema for "bad input argument".
 *
 * Algorithm
 * ---------
 *
 * Identical to set_si but with sign forced to +1:
 *
 *   1. Validate `prec` / `rnd` / `n` at the boundary.
 *   2. If `n === 0n`: return `{value: posZero(prec), ternary: 0}`.
 *   3. Otherwise:
 *      - `srcPrec = bitLength(n)`         // 1..64
 *      - `srcMant = n`                    // already MSB-aligned
 *      - `srcExp = srcPrec`
 *      - If `prec >= srcPrec`: lossless pad. Ternary 0.
 *      - If `prec < srcPrec`: delegate to `roundMantissa(...)` with
 *        sign = +1.
 *
 * The same comments on bit-length helper, lossless-pad correctness,
 * and substrate carry-out handling that apply to set_si.ts apply here.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_ui.c L25–L29 — C reference (trivial delegate).
 *   - mpfr/src/set_ui_2exp.c L26–L90 — the load-bearing implementation.
 *   - mpfr/src/round_raw_generic.c — canonical rounding primitive.
 *   - src/internal/mpfr/round_raw.ts — TS substrate counterpart.
 *   - src/ops/set_si.ts — signed sibling; this file is a direct
 *     specialisation for sign = +1 with widened range.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — same set as set_si.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Largest unsigned 64-bit integer: `2^64 - 1`. Matches `ULONG_MAX` on
 * the platforms we target (CLAUDE.md Rule 12: Bun ≥1.3 / Node ≥22, both
 * with 64-bit `unsigned long` on Linux x86_64 / aarch64).
 */
const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

/**
 * Bit length of a non-negative bigint (position of topmost set bit,
 * 1-indexed). Returns 0 for 0n. Bounded loop in ≤ 64 iterations given
 * the input domain. Duplicated from set_si.ts deliberately: the
 * helper is six lines and putting it in a third file is over-extraction.
 */
function bitLength(n: bigint): bigint {
  let bits = 0n;
  let probe = n;
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

/**
 * Validate the public-boundary scalar arguments. Throws `MPFRError`
 * on bad input. Mirror of set_si's validator, but the `n` range is
 * `[0, ULONG_MAX]`.
 */
function validateArgs(n: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < 0n || n > ULONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of uint64 range [0, ${ULONG_MAX_VAL}], got ${n}`,
    );
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Convert an unsigned 64-bit integer to an {@link MPFR} value at `prec`
 * bits, rounded per `rnd`.
 *
 * @mpfrName mpfr_set_ui
 *
 * @param n     the integer value, as a `bigint` in `[0, ULONG_MAX]`
 *              (`[0, 2^64 - 1]`). Use `BigInt(jsNumber)` to convert a
 *              JS number that is exactly representable as a non-negative
 *              integer.
 * @param prec  precision in **bits**.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per the {@link Result} shape.
 *
 * @throws {MPFRError} `EPREC` on bad precision or `n` outside uint64;
 *                    `EROUND` on bad rounding mode.
 *
 * @example
 *   mpfr_set_ui(0n, 53n, 'RNDN').value;             // posZero(53n)
 *   mpfr_set_ui(1n, 53n, 'RNDN').value;             // +1.0
 *   mpfr_set_ui(5n, 2n, 'RNDN');                    // ties-to-even → 4
 *   mpfr_set_ui(ULONG_MAX_VAL, 53n, 'RNDN');        // 2^64 - 1 rounded to 53 bits
 */
export function mpfr_set_ui(
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(n, prec, rnd);

  // --- Zero shortcut --------------------------------------------------------
  // mpfr/src/set_ui_2exp.c L29–L35: `MPFR_SET_POS(x); if (i == 0)
  // { MPFR_SET_ZERO(x); return 0; }`. The C path forces sign +1 (which
  // is the only sign an unsigned integer can have anyway).
  if (n === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  // --- Non-zero path -------------------------------------------------------
  // n > 0 is guaranteed here; sign is always +1. The structure matches
  // set_si.ts exactly (see that file's commentary for the schema
  // bookkeeping on the lossless-pad and lossy-round branches).
  const srcPrec = bitLength(n);
  const srcMant = n;
  const srcExp = srcPrec;

  if (prec >= srcPrec) {
    const padShift = prec - srcPrec;
    const value: MPFR = {
      kind: 'normal',
      sign: 1,
      prec,
      exp: srcExp,
      mant: srcMant << padShift,
    };
    return { value, ternary: 0 };
  }

  const { mant, exp, ternary } = roundMantissa(
    srcMant,
    srcPrec,
    srcExp,
    prec,
    1,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign: 1,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
