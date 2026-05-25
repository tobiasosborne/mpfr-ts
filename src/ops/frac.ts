/**
 * ops/frac.ts -- pure-TS port of MPFR's `mpfr_frac`.
 *
 * Fractional part of an MPFR value: `frac(u) = u - trunc(u)`, with the
 * sign of `u` preserved (so `frac(-2.7) = -0.7`, not `+0.3`). The
 * returned value is correctly rounded to the target precision in the
 * given rounding mode.
 *
 * C signature
 * -----------
 *
 *   int mpfr_frac(mpfr_ptr r, mpfr_srcptr u, mpfr_rnd_t rnd_mode);
 *
 * Ref: mpfr/src/frac.c L29-L143.
 *
 * TS signature
 * ------------
 *
 *   mpfr_frac(u: MPFR, prec: bigint, rnd: RoundingMode): Result
 *
 * Algorithm: delegation via sub + trunc
 * -------------------------------------
 *
 * The C body extracts the fractional bits below the binary point with
 * count-leading-zeros + mpn_lshift then rounds. We compose existing
 * ops instead:
 *
 *   frac(u, prec, rnd) = sub(u, trunc(u, u.prec, 'RNDN'), prec, rnd)
 *
 * `mpfr_trunc(u, u.prec)` is exact (it just zeros bits below the
 * binary point at u's own precision), so the sub's ternary is exactly
 * the correctly-rounded ternary of `u - trunc(u)`. The rounding mode
 * passed to trunc is irrelevant because trunc has no rounding-mode
 * parameter in the TS surface (direction is fixed: toward zero).
 *
 * Special cases mirror mpfr/src/frac.c L43-L57:
 *
 *   - NaN              -> NaN, ternary 0.
 *   - +/-Inf           -> signed zero (sign of u), ternary 0.
 *   - integer u        -> signed zero (sign of u), ternary 0.
 *   - |u| < 1 (exp<=0) -> mpfr_set(u, prec, rnd) (u is its own fp).
 *   - otherwise        -> delegated sub-trunc above.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/frac.c L29-L143 -- C reference body.
 *   - src/core.ts -- locked MPFR, Result, RoundingMode, MPFRError.
 *   - src/ops/trunc.ts -- delegate (exact at u.prec).
 *   - src/ops/sub.ts -- delegate (does the rounding).
 *   - src/ops/set.ts -- |u| < 1 fast path.
 *   - src/ops/integer_p.ts -- integer fast path predicate.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negZero,
  posZero,
} from '../core.ts';
import { mpfr_integer_p } from './integer_p.ts';
import { mpfr_set } from './set.ts';
import { mpfr_sub } from './sub.ts';
import { mpfr_trunc } from './trunc.ts';

const VALID_RND: ReadonlySet<RoundingMode> = new Set<RoundingMode>([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
]);

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
  if (!VALID_RND.has(rnd)) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Fractional part of `u` rounded to `prec` bits in mode `rnd`.
 *
 * @mpfrName mpfr_frac
 *
 * @param u    operand (any kind).
 * @param prec output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd  one of the five RoundingMode values.
 *
 * @returns `{value, ternary}` -- the fractional part rounded per rnd
 *          and the ternary flag (sign of rounded - exact).
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on unknown rnd.
 */
export function mpfr_frac(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // Ref: mpfr/src/frac.c L43-L47 -- NaN -> NaN, ternary 0.
  if (u.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // Ref: mpfr/src/frac.c L48-L53 -- Inf or integer u -> signed zero,
  // ternary 0. Signed zero preserves u.sign (MPFR_SET_SAME_SIGN).
  if (u.kind === 'inf' || mpfr_integer_p(u)) {
    return {
      value: u.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // At this point u is a non-integer normal value (zero was integer-p).
  // Ref: mpfr/src/frac.c L55-L57 -- |u| < 1 -> mpfr_set(r, u, rnd).
  if (u.exp <= 0n) {
    return mpfr_set(u, prec, rnd);
  }

  // Ref: mpfr/src/frac.c L59-L142 -- general path. Compose:
  //   trunc(u) at u.prec is exact (zeros bits below the binary point);
  //   sub(u, trunc_u, prec, rnd) yields the correctly-rounded fractional
  //   part with the correct ternary by construction.
  const truncU = mpfr_trunc(u, u.prec).value;
  return mpfr_sub(u, truncU, prec, rnd);
}
