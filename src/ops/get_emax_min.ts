/**
 * ops/get_emax_min.ts -- pure-TS port of MPFR's `mpfr_get_emax_min`.
 *
 * Returns the smallest legal value the active emax (maximum exponent) may
 * take. The C body is a literal `return MPFR_EMAX_MIN`, where
 * `MPFR_EMAX_MIN = 1 - MPFR_EXP_INVALID = 1 - 2^62` on 64-bit hosts.
 *
 * Algorithm (mpfr/src/exceptions.c L88-L92):
 *
 *   return MPFR_EMAX_MIN
 *
 * Ref: mpfr/src/exceptions.c L88-L92 -- C reference body.
 * Ref: mpfr/src/mpfr-impl.h L1050    -- MPFR_EMAX_MIN.
 *
 * @divergence C returns `mpfr_exp_t` (long); TS returns `bigint`. Value
 *   hardcoded to `1 - 2^62 = -4611686018427387903n`.
 */

import type { MPFR as _MPFR } from '../core.ts';

const EMAX_MIN: bigint = 1n - (1n << 62n);

/**
 * Smallest legal value of the active emax.
 *
 * @mpfrName mpfr_get_emax_min
 *
 * @returns `-4611686018427387903n` (= 1 - 2^62).
 */
export function mpfr_get_emax_min(): bigint {
  return EMAX_MIN;
}
