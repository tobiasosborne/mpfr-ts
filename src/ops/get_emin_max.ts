/**
 * ops/get_emin_max.ts -- pure-TS port of MPFR's `mpfr_get_emin_max`.
 *
 * Returns the largest legal value the active emin (minimum exponent) may
 * take. The C body is a literal `return MPFR_EMIN_MAX`, where
 * `MPFR_EMIN_MAX = MPFR_EXP_INVALID - 1 = +(2^62 - 1)` on 64-bit hosts.
 *
 * Algorithm (mpfr/src/exceptions.c L58-L62):
 *
 *   return MPFR_EMIN_MAX
 *
 * Ref: mpfr/src/exceptions.c L58-L62 -- C reference body.
 * Ref: mpfr/src/mpfr-impl.h L1049    -- MPFR_EMIN_MAX.
 *
 * @divergence C returns `mpfr_exp_t` (long); TS returns `bigint`. Value
 *   hardcoded to `2^62 - 1 = 4611686018427387903n`.
 */

import type { MPFR as _MPFR } from '../core.ts';

const EMIN_MAX: bigint = (1n << 62n) - 1n;

/**
 * Largest legal value of the active emin.
 *
 * @mpfrName mpfr_get_emin_max
 *
 * @returns `4611686018427387903n` (= 2^62 - 1).
 */
export function mpfr_get_emin_max(): bigint {
  return EMIN_MAX;
}
