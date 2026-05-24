/**
 * ops/abs.ts — pure-TS port of MPFR's `mpfr_abs`.
 *
 * Take the absolute value of an {@link MPFR} at the caller-supplied
 * target precision, rounded per the rounding mode, returning the
 * canonical `{value, ternary}` shape from src/core.ts.
 *
 * C signature
 * -----------
 *
 *   int mpfr_abs(mpfr_t rop, mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - sets `rop` to `|op|` (rounded if `prec(rop) != prec(op)`);
 *   - returns the ternary as the function result.
 *
 *   The C side declares this as a macro
 *
 *     #define mpfr_abs(a, b, r)  mpfr_set4(a, b, r, 1)
 *
 *   in mpfr/src/mpfr.h L970, and provides a same-name out-of-line
 *   definition in mpfr/src/set.c L83–L96 for the alias case (a == b):
 *
 *     if (a != b)
 *       return mpfr_set4(a, b, rnd_mode, MPFR_SIGN_POS);
 *     else {
 *       MPFR_SET_POS(a);
 *       if (MPFR_IS_NAN(b)) MPFR_RET_NAN;
 *       else MPFR_RET(0);
 *     }
 *
 *   — i.e. the general path is "copy b's mantissa into a (rounding to
 *   a's prec) and impose sign = +1"; the in-place path just clears the
 *   sign bit (always exact, ternary 0).
 *
 * TS signature
 * ------------
 *
 *   mpfr_abs(x: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `prec` as an explicit positional argument (no `rop` alias);
 *   - returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Algorithm
 * ---------
 *
 * Top-level dispatch on `x.kind`. The result sign is **always `+1`** for
 * every non-NaN kind:
 *
 *   1. NaN: return `{value: NAN_VALUE, ternary: 0}`. The canonical
 *      NAN_VALUE already has `sign = 1`; nothing else to do.
 *
 *   2. ±Inf: result is `+Inf`. Exact, ternary 0.
 *
 *   3. ±0: result is `+0`. Exact, ternary 0. The C reference treats
 *      `|-0| = +0` (sign cleared); our schema matches (mpfr/src/set.c
 *      L87: `MPFR_SET_POS(a)` after the alias branch, and the general
 *      path passes `MPFR_SIGN_POS` to mpfr_set4 which forces the result
 *      sign positive regardless of the source).
 *
 *   4. normal: sign = `+1`. If `x.sign` was already `+1`, the value is
 *      `x` rounded to `prec`; if `x.sign` was `-1`, the value is the
 *      magnitude of `x` rounded to `prec` (mantissa unchanged, sign
 *      flipped). Either way:
 *
 *        - If `prec >= x.prec`: lossless pad. New mantissa is
 *          `x.mant << (prec - x.prec)`, exponent unchanged, ternary 0.
 *
 *        - If `prec <  x.prec`: round `x.mant` from `x.prec` bits down
 *          to `prec` bits via the shared `roundMantissa` primitive.
 *          The rounding direction uses the **NEW sign (+1)**, exactly
 *          as the C reference does by passing `MPFR_SIGN_POS` to
 *          `mpfr_set4` (mpfr/src/set.c L87 + L58's MPFR_RNDRAW signb
 *          parameter).
 *
 * Why the new-sign rounding direction matters (mirrors neg.ts)
 * ------------------------------------------------------------
 *
 * For `mpfr_abs(x, prec, 'RNDU')` where `x` is a negative normal whose
 * mantissa rounds inexactly into lower precision:
 *
 *   - x = -1.1011 (5 bits)
 *   - target prec = 3 bits, rnd = RNDU.
 *
 * The exact intermediate `|x| = +1.1011`. Under RNDU (round toward +∞),
 * a positive value should be incremented when the rounding loses bits:
 * `+1.1011 → +1.110 (= 1.75)`, ternary +1. If we instead naively passed
 * the original `x.sign = -1` to `roundMantissa`, the RNDU branch would
 * route as "increment iff sign = +1 → no increment" → truncate to
 * `+1.101 (= 1.625)`, ternary -1 — incorrect (less than the exact
 * 1.6875, but RNDU should round UP, not down).
 *
 * Passing `+1` (the new sign) to `roundMantissa` reroutes correctly.
 * This is the same subtlety as in neg.ts — the C `mpfr_set4(..., signb)`
 * contract is "round per the OUTPUT's sign, not the source's".
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set.c L83–L96 — the out-of-line `mpfr_abs` for the
 *     alias case; the general macro at mpfr/src/mpfr.h L970 expands to
 *     `mpfr_set4(a, b, r, 1)`.
 *   - mpfr/src/set.c L25–L64 — `mpfr_set4`, the load-bearing primitive.
 *   - src/internal/mpfr/round_raw.ts — the shared substrate primitive.
 *   - src/ops/neg.ts — sibling op with structurally identical
 *     dispatch; differs only in the sign override (always +1 here;
 *     always -x.sign there).
 *   - src/core.ts — locked `MPFR` / `RoundingMode` / `Result` / `Sign`
 *     types; `posInf` / `posZero` / `NAN_VALUE`.
 *   - CLAUDE.md "Hallucination-risk callouts" — Signed zero observable
 *     (but `|±0| = +0` per MPFR_SIGN_POS); ternary direction is sign
 *     of (rounded - exact) AT THE RESULT SIGN.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  posInf,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Validate the public-boundary scalar arguments. Same shape as neg.ts /
 * add.ts / mul.ts.
 */
function validateArgs(prec: bigint, rnd: RoundingMode): void {
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
 * Take the absolute value of an MPFR at the target precision.
 *
 * @mpfrName mpfr_abs
 *
 * @param x     the operand. Any kind (`'normal'`, `'zero'`, `'inf'`, `'nan'`).
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. The value passes `validate()` without
 *              post-processing. Ternary is `0` for exact (every special,
 *              and every `prec >= x.prec` normal), `±1` only when the
 *              `prec < x.prec` rounding step lost bits.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf input is NOT an error.
 *
 * @example
 *   abs(setD(-3.14, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: +3.14 at prec 53, ternary: 0}
 *   abs(negZero(53n), 53n, 'RNDN');
 *     // → {value: posZero(53n), ternary: 0}  — |−0| = +0
 *   abs(negInf(53n), 53n, 'RNDN');
 *     // → {value: posInf(53n), ternary: 0}
 *   abs(NAN_VALUE, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}
 */
export function mpfr_abs(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  validateArgs(prec, rnd);

  // --- Specials --------------------------------------------------------
  // (1) NaN propagation.
  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) ±Inf → +Inf. Exact.
  if (x.kind === 'inf') {
    return { value: posInf(prec), ternary: 0 };
  }

  // (3) ±0 → +0. The C reference unconditionally sets sign=+1
  // (mpfr/src/set.c L87 + L29 in mpfr_set4). Signed zero is observable
  // in arithmetic, but `abs` deliberately collapses it.
  if (x.kind === 'zero') {
    return { value: posZero(prec), ternary: 0 };
  }

  // (4) Normal: force sign +1, then either lossless-pad (prec >= x.prec)
  // or round (prec < x.prec) the mantissa. The rounding uses the new
  // sign (+1), not the source's — see the docstring's "Why the
  // new-sign rounding direction matters" section.
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_abs: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  if (prec >= x.prec) {
    // Lossless: pad with zeros. Sign forced to +1.
    const padShift = prec - x.prec;
    const value: MPFR = {
      kind: 'normal',
      sign: 1,
      prec,
      exp: x.exp,
      mant: x.mant << padShift,
    };
    return { value, ternary: 0 };
  }

  // Lossy: round to fewer bits, pass +1 as the sign so RNDU/RNDD route
  // through roundMantissa as the positive-value branches.
  const { mant, exp, ternary } = roundMantissa(
    x.mant,
    x.prec,
    x.exp,
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
