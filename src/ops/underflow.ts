/**
 * ops/underflow.ts — pure-TS port of MPFR's `mpfr_underflow`.
 *
 * The "underflow exception" constructor: produce the value MPFR's
 * rounding logic returns when a computation underflows, given the
 * caller's rounding mode and the sign the unrounded exact result would
 * have. The result is either `±0` (for rounding modes that round toward
 * zero relative to the sign) or `±min-finite` (for the other modes),
 * with the matching ternary flag.
 *
 * C signature
 * -----------
 *
 *   int mpfr_underflow(mpfr_ptr x, mpfr_rnd_t rnd_mode, int sign);
 *
 *   Body (mpfr/src/exceptions.c L396-L420):
 *
 *     MPFR_ASSERT_SIGN(sign);
 *     if (MPFR_IS_LIKE_RNDZ(rnd_mode, sign < 0)) {
 *       MPFR_SET_ZERO(x);
 *       inex = -1;
 *     } else {
 *       mpfr_setmin(x, __gmpfr_emin);
 *       inex = 1;
 *     }
 *     MPFR_SET_SIGN(x, sign);
 *     __gmpfr_flags |= MPFR_FLAGS_INEXACT | MPFR_FLAGS_UNDERFLOW;
 *     return sign > 0 ? inex : -inex;
 *
 *   In words:
 *     - If the rounding mode rounds toward zero with respect to the
 *       sign (RNDZ always, RNDD when sign>0, RNDU when sign<0): the
 *       returned value is ±0. Ternary `-1` for positive sign (rounded
 *       result is smaller than the tiny positive exact value), `+1`
 *       for negative sign.
 *     - Else (RNDN, RNDA, RNDD-when-sign<0, RNDU-when-sign>0): the
 *       returned value is ±min-finite (setmin at default emin). Ternary
 *       `+1` for positive sign, `-1` for negative sign.
 *
 *   The flag-side effects (`MPFR_FLAGS_INEXACT | MPFR_FLAGS_UNDERFLOW`)
 *   are NOT mirrored in the TS port — the locked schema is pure.
 *
 * TS signature
 * ------------
 *
 *   mpfr_underflow(prec: bigint, rnd: RoundingMode, sign: Sign): Result;
 *
 *   - `prec`: target precision (the C side reads this off `x`; we pass
 *     it explicitly so the returned MPFR carries the right precision).
 *   - `rnd`:  rounding mode (5 supported modes per src/core.ts).
 *   - `sign`: strict `1 | -1`. C accepts `int` and asserts non-zero;
 *     TS rejects anything that's not exactly `+1` or `-1` with
 *     `MPFRError('EPREC', ...)`.
 *   - Returns `{ value, ternary }` carrying both the rounded MPFR
 *     and the ternary the C side returns.
 *
 * Default emin
 * ------------
 *
 *   The C body calls `mpfr_setmin(x, __gmpfr_emin)` where
 *   `__gmpfr_emin` is the current (global) minimum exponent. In the TS
 *   port, we hard-code the default value:
 *
 *     MPFR_EMIN_DEFAULT = -(MPFR_EMAX_DEFAULT) = -(2^30 - 1)
 *
 *   Ref: mpfr/src/mpfr.h L231-L232 — MPFR_EMAX_DEFAULT and MPFR_EMIN_DEFAULT.
 *
 * Divergence from C → TS
 * ----------------------
 *
 *   - Returns a fresh value (no in-place mutation); C mutates `x`.
 *   - Returns `Result` (value+ternary); C returns the ternary as int.
 *   - No global flag side effects (the locked schema is pure).
 *   - Strict `Sign` discriminant instead of `int` with assert.
 *   - Validates `prec` and `rnd` at the public-API boundary.
 *
 * Ternary logic
 * -------------
 *
 *   Ternary is the sign of (rounded - exact). For underflow:
 *   - ±0 branch: the rounded result is ±0, the exact result is a tiny
 *     nonzero value with the same sign. For positive sign:
 *     rounded (0) < exact (tiny +), so ternary = -1.
 *     For negative sign: rounded (−0) > exact (tiny −), so ternary = +1.
 *   - ±min-finite branch: the rounded result is the smallest finite
 *     representable value, the exact result is strictly smaller in
 *     magnitude. For positive sign: rounded (min+) > exact (tiny +),
 *     so ternary = +1. For negative sign: rounded (−min) < exact
 *     (tiny −), so ternary = −1.
 *
 *   In both cases: final ternary = `sign > 0 ? inex : -inex`.
 *   For ±0 branch: inex = -1. For ±min-finite branch: inex = +1.
 *   This matches the C formula at L419.
 *
 * Signed zero
 * -----------
 *
 *   The ±0 branch must carry the input `sign` per the locked schema
 *   (CLAUDE.md "Signed zero is real"). Positive sign → +0; negative
 *   sign → −0.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/exceptions.c L396-L420 — the C reference body.
 *   - mpfr/src/mpfr.h L231-L232 — MPFR_EMAX_DEFAULT / MPFR_EMIN_DEFAULT.
 *   - mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro definition.
 *   - src/ops/setmin.ts — delegate for the min-finite branch.
 *   - src/ops/overflow.ts — sibling exception constructor (mirror image).
 *   - src/core.ts — RoundingMode, Sign, Result, posZero, negZero.
 *   - CLAUDE.md "Signed zero is real" — the ±0 carries the input sign; do not collapse.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../core.ts';
import {
  MPFRError,
  negZero,
  posZero,
  PREC_MAX,
  PREC_MIN,
} from '../core.ts';
import { mpfr_setmin } from './setmin.ts';

/**
 * Default minimum exponent. Mirrors `MPFR_EMIN_DEFAULT` from
 * `mpfr/src/mpfr.h` L232:
 *
 *   #define MPFR_EMIN_DEFAULT (-(MPFR_EMAX_DEFAULT))
 *   #define MPFR_EMAX_DEFAULT ((mpfr_exp_t) (((mpfr_ulong) 1 << 30) - 1))
 *
 * So EMIN_DEFAULT = -(2^30 - 1).
 *
 * Ref: mpfr/src/mpfr.h L231-L232.
 */
const EMIN_DEFAULT: bigint = -((1n << 30n) - 1n);

/** Frozen tuple of valid rounding modes — same shape as overflow.ts. */
const VALID_RND: readonly RoundingMode[] = Object.freeze([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
] as const);

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ (mpfr-impl.h L1233-L1234): does
 * `rnd` round toward zero with respect to the sign?
 *
 *   - RNDZ always rounds toward zero.
 *   - RNDU rounds toward +∞, which is "toward zero" when sign < 0.
 *   - RNDD rounds toward -∞, which is "toward zero" when sign > 0.
 *   - RNDN and RNDA never round toward zero.
 *
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234 — the C macro definition:
 *   MPFR_IS_LIKE_RNDZ(rnd, neg) = ((rnd)==MPFR_RNDZ || (rnd+neg)==MPFR_RNDD)
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  return false;
}

/**
 * Validate the public-boundary arguments. Mirrors the gate convention
 * from sibling ops (overflow.ts / setmin.ts / set_inf.ts).
 */
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
      `mpfr_underflow: unknown rounding mode '${String(rnd)}'`,
    );
  }
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_underflow: sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

/**
 * Construct the underflow-exception value for the given precision,
 * rounding mode, and sign.
 *
 * @mpfrName mpfr_underflow
 *
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   rounding mode (5 supported modes).
 * @param sign  strict `1` or `-1`.
 *
 * @returns     `{ value, ternary }` where:
 *              - `value` is `±0` if the rounding mode rounds toward
 *                zero w.r.t. the sign, else `±min-finite` (at the
 *                default emin).
 *              - `ternary` is the C function's return value (sign of
 *                rounded - exact). Per the C code, the ternary is never
 *                `0` for an underflow — it is either `-1` or `+1`.
 *
 * @throws {MPFRError} `EPREC` on bad precision or non-`Sign` sign;
 *                    `EROUND` on unknown rounding mode.
 *
 * @example
 *   mpfr_underflow(53n, 'RNDN', 1)   // { value: +min, ternary: 1 }
 *   mpfr_underflow(53n, 'RNDZ', 1)   // { value: +0,   ternary: -1 }
 *   mpfr_underflow(53n, 'RNDU', -1)  // { value: -0,   ternary: 1 }
 *   mpfr_underflow(53n, 'RNDD', -1)  // { value: -min, ternary: -1 }
 */
export function mpfr_underflow(
  prec: bigint,
  rnd: RoundingMode,
  sign: Sign,
): Result {
  validateArgs(prec, rnd, sign);

  // Branch on the MPFR_IS_LIKE_RNDZ predicate.
  // Ref: mpfr/src/exceptions.c L407-L416.
  const towardZero = isLikeRNDZ(rnd, sign);

  let value: MPFR;
  let inex: Ternary;
  if (towardZero) {
    // Zero branch: the rounded value is ±0 (toward-zero from the tiny
    // exact nonzero value). Inex = -1 (set before applying sign).
    // Signed zero is preserved per the locked schema: sign=1 → +0, sign=-1 → -0.
    // Ref: mpfr/src/exceptions.c L408-L410.
    value = sign === 1 ? posZero(prec) : negZero(prec);
    inex = -1;
  } else {
    // Min-finite branch: the rounded value is the smallest finite
    // representable value at this precision, at the default emin.
    // Ref: mpfr/src/exceptions.c L413-L416.
    value = mpfr_setmin(prec, EMIN_DEFAULT, sign);
    inex = 1;
  }

  // C returns `sign > 0 ? inex : -inex`. Equivalent to multiplying inex
  // by sign. Both inex values are in {-1, +1} so the product is too.
  // Ref: mpfr/src/exceptions.c L419.
  const ternary: Ternary = (sign === 1 ? inex : -inex) as Ternary;

  return { value, ternary };
}
