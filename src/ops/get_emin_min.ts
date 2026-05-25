/**
 * ops/get_emin_min.ts -- pure-TS port of MPFR's `mpfr_get_emin_min`.
 *
 * Returns the smallest legal value the active emin (minimum exponent) may
 * take. The C body is a literal `return MPFR_EMIN_MIN`, where
 * `MPFR_EMIN_MIN = 1 - MPFR_EXP_INVALID = 1 - 2^62` on 64-bit hosts.
 *
 * Algorithm (mpfr/src/exceptions.c L52-L56):
 *
 *   return MPFR_EMIN_MIN
 *
 * Ref: mpfr/src/exceptions.c L52-L56 -- C reference body.
 * Ref: mpfr/src/mpfr-impl.h L1048    -- MPFR_EMIN_MIN = 1 - MPFR_EXP_INVALID.
 * Ref: mpfr/src/mpfr-impl.h L1036-L1037 -- MPFR_EXP_INVALID = 2^62.
 *
 * @divergence C returns `mpfr_exp_t` (long); TS returns `bigint`. Value
 *   hardcoded to `1 - 2^62 = -4611686018427387903n`.
 */

import type { MPFR as _MPFR } from '../core.ts';

const EMIN_MIN: bigint = 1n - (1n << 62n);

/**
 * Smallest legal value of the active emin.
 *
 * @mpfrName mpfr_get_emin_min
 *
 * @returns `-4611686018427387903n` (= 1 - 2^62).
 */
export function mpfr_get_emin_min(): bigint {
  return EMIN_MIN;
}
