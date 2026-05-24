/**
 * ops/overflow.ts — pure-TS port of MPFR's `mpfr_overflow`.
 *
 * The "overflow exception" constructor: produce the value MPFR's
 * rounding logic returns when a computation overflows, given the
 * caller's rounding mode and the sign the unrounded exact result
 * would have. The result is either `±max-finite` (for rounding modes
 * that round toward zero relative to the sign) or `±Inf` (for the
 * other modes), with the matching ternary flag.
 *
 * Mirror image of {@link mpfr_underflow}: same MPFR_IS_LIKE_RNDZ branch,
 * but the "toward-zero" branch produces ±max-finite instead of ±0,
 * and the "away-from-zero" branch produces ±Inf instead of ±min-finite.
 *
 * C signature
 * -----------
 *
 *   int mpfr_overflow(mpfr_ptr x, mpfr_rnd_t rnd_mode, int sign);
 *
 *   Body (mpfr/src/exceptions.c L424-L448):
 *
 *     MPFR_ASSERT_SIGN(sign);
 *     if (MPFR_IS_LIKE_RNDZ(rnd_mode, sign < 0)) {
 *       mpfr_setmax(x, __gmpfr_emax);
 *       inex = -1;
 *     } else {
 *       MPFR_SET_INF(x);
 *       inex = 1;
 *     }
 *     MPFR_SET_SIGN(x, sign);
 *     __gmpfr_flags |= MPFR_FLAGS_INEXACT | MPFR_FLAGS_OVERFLOW;
 *     return sign > 0 ? inex : -inex;
 *
 *   In words:
 *     - If the rounding mode rounds toward zero with respect to the
 *       sign (RNDZ always, RNDD when sign>0, RNDU when sign<0): the
 *       returned value is ±max-finite (mpfr_setmax at the default emax).
 *       Ternary `-1` for positive sign (rounded value is smaller than
 *       the exact positive overflow target), `+1` for negative sign.
 *     - Else (RNDN, RNDA, RNDU-when-sign>0, RNDD-when-sign<0): the
 *       returned value is ±Inf. Ternary `+1` for positive sign,
 *       `-1` for negative sign.
 *
 *   The flag-side effects (`MPFR_FLAGS_INEXACT | MPFR_FLAGS_OVERFLOW`)
 *   are NOT mirrored in the TS port — the locked schema is pure.
 *
 * TS signature
 * ------------
 *
 *   mpfr_overflow(prec: bigint, rnd: RoundingMode, sign: Sign): Result;
 *
 *   - `prec`: target precision (the C side reads this off `x`).
 *   - `rnd`:  rounding mode (5 supported modes per src/core.ts).
 *   - `sign`: strict `1 | -1`.
 *   - Returns `{ value, ternary }`.
 *
 * Default emax is `MPFR_EMAX_DEFAULT = 2^30 - 1`
 * (mpfr/src/mpfr.h L231). The TS port does not expose emax management,
 * so this constant is the only emax in play.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate inputs.
 *   2. Decide between "±max-finite" and "±Inf" branches per
 *      {@link isLikeRNDZ}.
 *   3. Build the appropriate MPFR value:
 *      - ±max-finite branch: delegate to {@link mpfr_setmax}(prec,
 *        EMAX_DEFAULT, sign).
 *      - ±Inf branch: delegate to {@link posInf} / {@link negInf}.
 *   4. Compute ternary identically to mpfr_underflow: sign-multiplied
 *      inex.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/exceptions.c L424-L448 — the C reference body.
 *   - mpfr/src/mpfr.h L231 — MPFR_EMAX_DEFAULT = 2^30 - 1.
 *   - mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 *   - src/ops/setmax.ts — delegate for the max-finite branch.
 *   - src/ops/underflow.ts — sibling exception constructor (mirror image).
 *   - src/core.ts L137-L151 — RoundingMode.
 *   - src/core.ts L75-L90 — Sign discriminant.
 *   - CLAUDE.md "Ternary flag is the sign of (rounded - exact)" —
 *     direction is critical.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import {
  MPFRError,
  negInf,
  posInf,
  PREC_MAX,
  PREC_MIN,
} from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import { mpfr_setmax } from '/home/tobias/Projects/mpfr-ts/src/ops/setmax.ts';

/**
 * Default exponent ceiling. Mirrors `MPFR_EMAX_DEFAULT = (1 << 30) - 1`
 * from mpfr/src/mpfr.h L231.
 *
 * Ref: mpfr/src/mpfr.h L231 —
 *   `#define MPFR_EMAX_DEFAULT ((mpfr_exp_t) ((((mpfr_uexp_t) 1) << 30) - 1))`.
 */
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n;

const VALID_RND: readonly RoundingMode[] = Object.freeze([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
] as const);

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ — true when the rounding mode rounds
 * toward zero with respect to the given sign.
 *
 * From the macro (mpfr/src/mpfr-impl.h L1233-L1234):
 *   MPFR_IS_LIKE_RNDZ(rnd, neg) where neg = (sign < 0):
 *     rnd == RNDZ
 *     || (rnd == RNDU && neg)   [neg=1 means sign<0, RNDU rounds toward -inf for negatives => toward zero]
 *     || (rnd == RNDD && !neg)  [neg=0 means sign>0, RNDD rounds toward -inf for positives => toward zero]
 *
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  // neg = (sign < 0) = (sign === -1)
  if (rnd === 'RNDU' && sign === -1) return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  return false;
}

function validateArgs(prec: bigint, rnd: RoundingMode, sign: Sign): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (!VALID_RND.includes(rnd)) {
    throw new MPFRError(
      'EROUND',
      `mpfr_overflow: unknown rounding mode '${String(rnd)}'`,
    );
  }
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_overflow: sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

/**
 * Construct the overflow-exception value for the given precision,
 * rounding mode, and sign.
 *
 * @mpfrName mpfr_overflow
 *
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   rounding mode.
 * @param sign  strict `1` or `-1`.
 *
 * @returns     `{ value, ternary }` where:
 *              - `value` is `±max-finite` if the rounding mode rounds
 *                toward zero w.r.t. the sign, else `±Inf`.
 *              - `ternary` is `-1` or `+1` per the C reference.
 *
 * @throws {MPFRError} `EPREC` / `EROUND` on bad inputs.
 *
 * @example
 *   mpfr_overflow(53n, 'RNDN', 1)   // { value: +Inf,     ternary: 1 }
 *   mpfr_overflow(53n, 'RNDZ', 1)   // { value: +max,     ternary: -1 }
 *   mpfr_overflow(53n, 'RNDU', -1)  // { value: -max,     ternary: 1 }
 *   mpfr_overflow(53n, 'RNDD', -1)  // { value: -Inf,     ternary: -1 }
 */
export function mpfr_overflow(
  prec: bigint,
  rnd: RoundingMode,
  sign: Sign,
): Result {
  validateArgs(prec, rnd, sign);

  // Ref: mpfr/src/exceptions.c L435-L444.
  const towardZero = isLikeRNDZ(rnd, sign);

  let value: MPFR;
  let inex: Ternary;
  if (towardZero) {
    // ±max-finite branch: mant = 2^prec - 1, exp = emax, sign per arg.
    // Ref: mpfr/src/exceptions.c L437 — mpfr_setmax(x, __gmpfr_emax).
    value = mpfr_setmax(prec, EMAX_DEFAULT, sign);
    inex = -1;
  } else {
    // ±Inf branch.
    // Ref: mpfr/src/exceptions.c L441 — MPFR_SET_INF(x).
    value = sign === 1 ? posInf(prec) : negInf(prec);
    inex = 1;
  }

  // C returns `sign > 0 ? inex : -inex`.
  // Ref: mpfr/src/exceptions.c L447.
  const ternary: Ternary = (sign === 1 ? inex : -inex) as Ternary;

  return { value, ternary };
}
