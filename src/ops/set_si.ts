/**
 * ops/set_si.ts — pure-TS port of MPFR's `mpfr_set_si`.
 *
 * Convert a machine signed integer to an {@link MPFR} value at the
 * caller-supplied precision, rounded per the rounding mode, returning
 * the canonical `{value, ternary}` shape.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_si(mpfr_t rop, long int op, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - returns the ternary as the function result.
 *
 *   The C implementation (mpfr/src/set_si.c L25–L29) trivially delegates
 *   to `mpfr_set_si_2exp(rop, i, 0, rnd_mode)`, whose load-bearing body
 *   lives in mpfr/src/set_si_2exp.c L27–L92:
 *
 *     1. If `i == 0`: store +0, ternary 0.
 *     2. Otherwise: take the absolute value `|i|`, count leading zeros,
 *        place the mantissa MSB-aligned at the top limb (xp[xn] = |i| <<
 *        cnt), assign the sign (i < 0 → MPFR_SIGN_NEG), set the
 *        pre-rounding exponent `e = nbits = GMP_NUMB_BITS - cnt`.
 *     3. If `MPFR_PREC(x) < nbits` — i.e. the requested precision is
 *        narrower than the integer's bit length — round via
 *        `mpfr_round_raw` (with `signb = (i < 0)`) and bump the exponent
 *        on carry-out.
 *
 *   We mirror this exactly in single-bigint form, delegating the round
 *   step to the shared `roundMantissa` substrate
 *   (src/internal/mpfr/round_raw.ts).
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_si(n: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `n` as a `bigint` because JS `number` cannot losslessly hold
 *     the full int64 range (`Number.MAX_SAFE_INTEGER === 2^53 - 1`,
 *     short of LONG_MAX). Callers with a small Number can pre-convert
 *     with `BigInt(n)`.
 *   - takes `prec` as an explicit positional argument (no `rop`);
 *   - returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Range
 * -----
 *
 * `n` is required to lie in `[LONG_MIN, LONG_MAX]` — i.e.
 * `[-(2^63), 2^63 - 1]`. Out-of-range `n` throws `MPFRError('EPREC',
 * ...)` (we reuse `EPREC` for "bad input argument" — there's no
 * dedicated `ERANGE` discriminant in the locked schema; out-of-range
 * integer is reported as a precision-shaped error since both indicate
 * the caller passed a value outside the function's documented input
 * domain). The C function takes a literal `long`, so values outside
 * the platform's `long` range can't be passed without an explicit
 * cast and undefined behaviour; making the TS analog throw is the
 * safer choice.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `prec` / `rnd` / `n` at the boundary.
 *   2. If `n === 0n`: return `{value: posZero(prec), ternary: 0}` (the
 *      C reference forces sign +1 on zero — mpfr/src/set_si_2exp.c L31–L33).
 *   3. Otherwise:
 *      - `sign = n < 0n ? -1 : 1`
 *      - `absN = n < 0n ? -n : n`        // bigint negation is exact
 *      - `srcPrec = bitLength(absN)`     // 1..64
 *      - `srcMant = absN << (srcPrec - bitLength)` — already MSB-aligned
 *        because `absN`'s top bit is at position `srcPrec - 1`. So
 *        `srcMant === absN` and `srcMant ∈ [2^(srcPrec-1), 2^srcPrec)`.
 *      - `srcExp = srcPrec` (the value's magnitude lies in `[2^(srcPrec-1),
 *        2^srcPrec)` per the schema's `[2^(exp-1), 2^exp)` convention).
 *      - If `prec >= srcPrec`: lossless. Pad the mantissa with zeros via
 *        `srcMant << (prec - srcPrec)`. Ternary 0.
 *      - If `prec < srcPrec`: delegate to `roundMantissa(srcMant,
 *        srcPrec, srcExp, prec, sign, rnd)`. The substrate handles
 *        carry-out (mantissa overflowing to `2^prec`) by renormalising
 *        and bumping the exponent.
 *
 * Bit-length helper
 * -----------------
 *
 * JavaScript's `bigint` has no built-in `bitLength`. We use a binary
 * doubling loop bounded by 64 iterations (the int64 input domain caps
 * the bit length at 64). Cheap and branch-predictable; profile-blind
 * but the integer-conversion path is not perf-critical.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_si.c L25–L29 — C reference (trivial delegate).
 *   - mpfr/src/set_si_2exp.c L27–L92 — the load-bearing implementation.
 *   - mpfr/src/round_raw_generic.c — canonical rounding primitive.
 *   - src/internal/mpfr/round_raw.ts — TS substrate counterpart.
 *   - src/core.ts — locked MPFR / RoundingMode / Result / Sign types.
 *   - src/ops/set_d.ts — sibling op for the double input path; shares
 *     the "MSB-align, lossless-pad or round" structure.
 *   - CLAUDE.md "Hallucination-risk callouts" — signed zero (the n=0
 *     branch forces +0 per the C reference); ternary direction is sign
 *     of (rounded - exact); rounding-mode count is FIVE.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Smallest signed 64-bit integer: `-(2^63)`. Matches the C `LONG_MIN`
 * for `_MPFR_PREC_FORMAT`-irrelevant int64 (the only platform the TS
 * port targets — see CLAUDE.md Rule 12).
 */
const LONG_MIN_VAL: bigint = -(1n << 63n);

/**
 * Largest signed 64-bit integer: `2^63 - 1`. Matches the C `LONG_MAX`
 * on the platforms we target.
 */
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

/**
 * Compute the bit length of a non-negative bigint (position of the
 * topmost set bit, 1-indexed). Returns 0 for `0n`. Bounded loop —
 * caller guarantees the input lies in `[0, 2^64)` so this terminates
 * in ≤ 64 iterations.
 *
 * No built-in `bitLength` on bigint in current ECMAScript; the manual
 * loop is the standard pattern. We use the doubling shift form rather
 * than `Math.log2(Number(n))` to stay precise at the upper end of the
 * int64 range (Number truncates above 2^53).
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
 * on bad input. Same shape as set_d / neg / abs.
 *
 * The `n` range check uses `EPREC` for the discriminant because the
 * locked schema (src/core.ts L186–L188) defines only three error
 * codes — EPREC / EROUND / EDOMAIN — and EPREC is the closest fit for
 * "bad argument to a public op". A future schema bump could introduce
 * a dedicated `ERANGE` code; until then EPREC + the message text
 * documents the failure unambiguously.
 */
function validateArgs(n: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < LONG_MIN_VAL || n > LONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of int64 range [${LONG_MIN_VAL}, ${LONG_MAX_VAL}], got ${n}`,
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
 * Convert a signed 64-bit integer to an {@link MPFR} value at `prec`
 * bits, rounded per `rnd`.
 *
 * @mpfrName mpfr_set_si
 *
 * @param n     the integer value, as a `bigint` in `[LONG_MIN, LONG_MAX]`
 *              (`[-(2^63), 2^63 - 1]`). Use `BigInt(jsNumber)` to convert
 *              a JS number that is exactly representable as an integer.
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`. Per
 *              the hallucination-risk callout, `53n` is 53 mantissa
 *              bits, not 53 decimal digits.
 * @param rnd   one of the five rounding modes in {@link RoundingMode}.
 *
 * @returns     a {@link Result} pair `{value, ternary}`.
 *              - `value` is a well-formed {@link MPFR} at the requested
 *                precision (passes `validate()` without post-processing).
 *              - `ternary` is the sign of `(value - exact)` — `0` for
 *                exact (every case where `prec >= bitLength(|n|)`, plus
 *                `n === 0n`), `±1` otherwise.
 *
 * @throws {MPFRError} `EPREC` on bad precision or `n` outside int64;
 *                    `EROUND` on bad rounding mode.
 *
 * @example
 *   mpfr_set_si(0n, 53n, 'RNDN').value;       // posZero(53n)
 *   mpfr_set_si(1n, 53n, 'RNDN').value;       // +1.0 at prec 53
 *   mpfr_set_si(-1n, 53n, 'RNDN').value;      // -1.0 at prec 53
 *   mpfr_set_si(5n, 2n, 'RNDN');              // 5 = 0b101 → round to 2 bits → 4 (ties-to-even)
 *   mpfr_set_si(LONG_MAX_VAL, 53n, 'RNDN');   // 2^63 - 1 rounded to 53 bits
 */
export function mpfr_set_si(
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(n, prec, rnd);

  // --- Zero shortcut --------------------------------------------------------
  // mpfr/src/set_si_2exp.c L29–L33: `if (i == 0) { MPFR_SET_ZERO(x);
  // MPFR_SET_POS(x); return 0; }`. The C reference forces sign +1 on
  // zero; signed zero is not preserved here because the input integer
  // can't carry a sign for zero (unlike the IEEE 754 double in set_d).
  if (n === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  // --- Sign / magnitude split ----------------------------------------------
  const sign: Sign = n < 0n ? -1 : 1;
  const absN: bigint = n < 0n ? -n : n;

  // bitLength(absN) ∈ [1, 64] since we've eliminated n=0 and the range
  // is bounded by int64. The mantissa absN is already MSB-aligned to
  // exactly bitLength bits (its top bit is at position bitLength - 1).
  const srcPrec = bitLength(absN);
  const srcMant = absN;
  // The value's magnitude is in [2^(srcPrec-1), 2^srcPrec), so the
  // schema-convention exponent (where |value| ∈ [2^(exp-1), 2^exp)) is
  // srcExp = srcPrec.
  const srcExp = srcPrec;

  // --- Lossless padding when prec >= srcPrec --------------------------------
  // We can keep the mantissa exactly and pad with zeros to widen it to
  // `prec` bits MSB-aligned. The schema's value formula is `sign * mant
  // * 2^(exp - prec)`; padding by `(prec - srcPrec)` zeros to the right
  // multiplies mant by `2^(prec - srcPrec)` and exp by... wait, exp
  // doesn't change (we're widening the prec frame, not shifting the
  // value). The padded form has mant = srcMant << (prec - srcPrec),
  // exp = srcExp, and the value is sign * mant * 2^(exp - prec) =
  // sign * srcMant * 2^(prec - srcPrec) * 2^(srcExp - prec) =
  // sign * srcMant * 2^(srcExp - srcPrec) — the original value. ✓
  if (prec >= srcPrec) {
    const padShift = prec - srcPrec;
    const value: MPFR = {
      kind: 'normal',
      sign,
      prec,
      exp: srcExp,
      mant: srcMant << padShift,
    };
    return { value, ternary: 0 };
  }

  // --- Lossy rounding when prec < srcPrec ----------------------------------
  // Delegate to the shared substrate. `roundMantissa` handles carry-out
  // by renormalising the mantissa and bumping the exponent on overflow.
  const { mant, exp, ternary } = roundMantissa(
    srcMant,
    srcPrec,
    srcExp,
    prec,
    sign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
