/**
 * reference_ports/correct/mpfr_get_emax_max.ts -- mutation-prove reference.
 *
 * MPFR_EMAX_MAX = MPFR_EXP_INVALID - 1 = +(2^62 - 1).
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

const EMAX_MAX = (1n << 62n) - 1n;

export function mpfr_get_emax_max(): bigint {
  return EMAX_MAX;
}
