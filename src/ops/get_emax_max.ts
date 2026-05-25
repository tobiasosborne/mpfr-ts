/**
 * ops/get_emax_max.ts -- pure-TS port of MPFR's `mpfr_get_emax_max`.
 *
 * Returns the largest legal value the active emax (maximum exponent) may
 * take. The C body is a literal `return MPFR_EMAX_MAX`, where
 * `MPFR_EMAX_MAX = MPFR_EXP_INVALID - 1 = +(2^62 - 1)` on the 64-bit
 * mpfr_exp_t that the host libmpfr is built with.
 *
 * Algorithm (mpfr/src/exceptions.c L94-L98):
 *
 *   return MPFR_EMAX_MAX
 *
 * Ref: mpfr/src/exceptions.c L94-L98 -- C reference body.
 * Ref: mpfr/src/mpfr-impl.h L1051    -- MPFR_EMAX_MAX = MPFR_EXP_INVALID - 1.
 *
 * @divergence C returns `mpfr_exp_t` (long); TS returns `bigint` per the
 *   locked-schema exponent convention. Value hardcoded to `2^62 - 1`
 *   matching the probed system libmpfr.
 */

import type { MPFR as _MPFR } from '../core.ts';

const EMAX_MAX: bigint = (1n << 62n) - 1n;

/**
 * Largest legal value of the active emax.
 *
 * @mpfrName mpfr_get_emax_max
 *
 * @returns `4611686018427387903n` (= 2^62 - 1).
 */
export function mpfr_get_emax_max(): bigint {
  return EMAX_MAX;
}
