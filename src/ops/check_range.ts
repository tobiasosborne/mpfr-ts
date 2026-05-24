/**
 * ops/check_range.ts — pure-TS port of MPFR's `mpfr_check_range`.
 *
 * Adjusts a value for exponent-range overflow/underflow. Returns the
 * same value if in range, or the canonical underflow/overflow result if
 * the exponent of a normal `x` falls outside [EMIN_DEFAULT, EMAX_DEFAULT].
 *
 * C signature
 * -----------
 *
 *   int mpfr_check_range(mpfr_ptr x, int t, mpfr_rnd_t rnd_mode);
 *
 *   Body: mpfr/src/exceptions.c L258-L327.
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_check_range(x: MPFR, t: Ternary, rnd: RoundingMode): Result
 *
 * Algorithm
 * ---------
 *
 *   1. If x is singular (NaN, ±0, ±Inf):
 *      - If x is Inf and t != 0: the C code sets MPFR_FLAGS_OVERFLOW (TS
 *        doesn't model flags, so this is a no-op). Fall through.
 *      - Return { value: x, ternary: t } unchanged (MPFR_RET(t)).
 *      Ref: mpfr/src/exceptions.c L285-L316.
 *
 *   2. If x is normal (non-singular):
 *      a. Read exp = x.exp (may be outside [emin, emax]).
 *         Ref: mpfr/src/exceptions.c L263.
 *      b. If exp < EMIN_DEFAULT (underflow path):
 *         - If rnd == 'RNDN', possibly switch to 'RNDZ':
 *             condition: exp + 1 < EMIN_DEFAULT   (|x| < 2^(emin-2))
 *               OR (x is a power of 2 AND (sign>0 ? t>=0 : t<=0))
 *           i.e. mpfr_powerof2_raw(x) && (MPFR_IS_NEG(x) ? t<=0 : t>=0)
 *           Ref: mpfr/src/exceptions.c L274-L279.
 *         - Delegate to mpfr_underflow(x.prec, rnd, x.sign).
 *         Ref: mpfr/src/exceptions.c L280.
 *      c. If exp > EMAX_DEFAULT (overflow path):
 *         - Delegate to mpfr_overflow(x.prec, rnd, x.sign).
 *         Ref: mpfr/src/exceptions.c L282-L283.
 *      d. Otherwise (in-range): return { value: x, ternary: t } unchanged.
 *         Ref: mpfr/src/exceptions.c L316 — MPFR_RET(t).
 *
 * Divergence from C
 * -----------------
 *
 *   - Pure functional: no mutation, no flag side-effects.
 *   - Uses default EMIN_DEFAULT = -(2^30 - 1), EMAX_DEFAULT = 2^30 - 1.
 *   - Ternary input `t` is validated; out-of-range t throws EPREC.
 *   - The Inf-with-t!=0 overflow-flag path is a no-op in TS (flags not modelled).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/exceptions.c L249-L327 — the C reference body.
 *   - mpfr/src/exceptions.c L260-L284 — main dispatch.
 *   - mpfr/src/exceptions.c L274-L280 — RNDN-to-RNDZ adjustment.
 *   - src/ops/underflow.ts — underflow delegate.
 *   - src/ops/overflow.ts — overflow delegate.
 *   - src/internal/mpfr/powerof2_raw.ts — power-of-2 test.
 *   - src/core.ts — locked schema.
 */

import type { MPFR, RoundingMode, Result, Ternary, Sign } from '../core.ts';
import { MPFRError } from '../core.ts';
import { mpfr_underflow } from './underflow.ts';
import { mpfr_overflow } from './overflow.ts';
import { mpfr_powerof2_raw } from '../internal/mpfr/powerof2_raw.ts';

/**
 * Default minimum exponent. Mirrors `MPFR_EMIN_DEFAULT = -(2^30 - 1)`.
 * Ref: mpfr/src/mpfr.h L231-L232.
 */
const EMIN_DEFAULT: bigint = -((1n << 30n) - 1n);

/**
 * Default maximum exponent. Mirrors `MPFR_EMAX_DEFAULT = (2^30 - 1)`.
 * Ref: mpfr/src/mpfr.h L231.
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
 * Validate all public-boundary arguments.
 */
function validateArgs(x: MPFR, t: Ternary, rnd: RoundingMode): void {
  // Validate x structurally (kind check only; full validate is expensive
  // and the runner calls it on output, not input).
  if (
    x.kind !== 'normal' &&
    x.kind !== 'zero' &&
    x.kind !== 'inf' &&
    x.kind !== 'nan'
  ) {
    throw new MPFRError('EPREC', `mpfr_check_range: invalid x.kind: ${String(x.kind)}`);
  }

  // Validate ternary.
  if (t !== -1 && t !== 0 && t !== 1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_check_range: t must be -1, 0, or 1, got ${String(t)}`,
    );
  }

  // Validate rounding mode.
  if (!VALID_RND.includes(rnd)) {
    throw new MPFRError(
      'EROUND',
      `mpfr_check_range: unknown rounding mode '${String(rnd)}'`,
    );
  }
}

/**
 * Adjust `x` for exponent-range overflow/underflow.
 *
 * @mpfrName mpfr_check_range
 *
 * @param x    An MPFR value whose exponent may lie outside [EMIN, EMAX].
 * @param t    The ternary from the prior operation (carried through on
 *             in-range or singular inputs).
 * @param rnd  The rounding mode applied when underflow/overflow is triggered.
 *
 * @returns  `{ value, ternary }`:
 *           - Singular or in-range normal: `{ value: x, ternary: t }`.
 *           - Underflow (exp < EMIN): delegates to `mpfr_underflow` (with
 *             possible RNDN→RNDZ adjustment per C L274-L279).
 *           - Overflow (exp > EMAX): delegates to `mpfr_overflow`.
 *
 * @throws {MPFRError} `EPREC` on invalid ternary; `EROUND` on unknown mode.
 */
export function mpfr_check_range(
  x: MPFR,
  t: Ternary,
  rnd: RoundingMode,
): Result {
  validateArgs(x, t, rnd);

  // Branch on singularity.
  // Ref: mpfr/src/exceptions.c L261 — `if (MPFR_LIKELY(!MPFR_IS_SINGULAR(x)))`.
  if (x.kind === 'normal') {
    // Non-singular (normal finite nonzero) value.
    // The C code reads EXP(x) directly — NOT via MPFR_GET_EXP, which
    // asserts the exponent is in range. The raw exponent may be outside
    // [emin, emax] here (that's the whole point of check_range).
    // Ref: mpfr/src/exceptions.c L263 — `mpfr_exp_t exp = MPFR_EXP(x)`.
    const exp = x.exp;
    const sign = x.sign;
    const prec = x.prec;

    if (exp < EMIN_DEFAULT) {
      // Underflow path.
      // Ref: mpfr/src/exceptions.c L266-L281.

      // Possibly switch RNDN to RNDZ.
      // Condition (C L275-L279):
      //   rnd_mode == MPFR_RNDN &&
      //   (exp + 1 < __gmpfr_emin ||
      //    (mpfr_powerof2_raw(x) &&
      //     (MPFR_IS_NEG(x) ? t <= 0 : t >= 0)))
      // "MPFR_IS_NEG(x)" means sign < 0, i.e. sign === -1.
      // Ref: mpfr/src/exceptions.c L274-L279.
      let effectiveRnd: RoundingMode = rnd;
      if (rnd === 'RNDN') {
        const smallerThanBoundary = exp + 1n < EMIN_DEFAULT;
        const atBoundaryAndTowardZero =
          mpfr_powerof2_raw(x) &&
          (sign === -1 ? t <= 0 : t >= 0);
        if (smallerThanBoundary || atBoundaryAndTowardZero) {
          effectiveRnd = 'RNDZ';
        }
      }

      // Delegate to mpfr_underflow.
      // Ref: mpfr/src/exceptions.c L280 — `return mpfr_underflow(x, rnd_mode, MPFR_SIGN(x))`.
      return mpfr_underflow(prec, effectiveRnd, sign as Sign);
    }

    if (exp > EMAX_DEFAULT) {
      // Overflow path.
      // Ref: mpfr/src/exceptions.c L282-L283 — `return mpfr_overflow(x, rnd_mode, MPFR_SIGN(x))`.
      return mpfr_overflow(prec, rnd, sign as Sign);
    }

    // In-range: return unchanged with the carried ternary.
    // Ref: mpfr/src/exceptions.c L316 — `MPFR_RET(t)`.
    return { value: x, ternary: t };
  }

  // Singular (NaN, ±0, ±Inf) path.
  // Ref: mpfr/src/exceptions.c L285-L316.
  // C sets MPFR_FLAGS_OVERFLOW when x is Inf and t != 0 (L285-L315),
  // but TS does not model flag state — that branch is a no-op here.
  // All paths fall through to MPFR_RET(t) which returns t unchanged.
  return { value: x, ternary: t };
}
