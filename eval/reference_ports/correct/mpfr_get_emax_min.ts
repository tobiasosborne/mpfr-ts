/**
 * reference_ports/correct/mpfr_get_emax_min.ts -- mutation-prove reference.
 *
 * MPFR_EMAX_MIN = 1 - MPFR_EXP_INVALID = 1 - 2^62 = -4611686018427387903n.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

const EMAX_MIN = 1n - (1n << 62n);

export function mpfr_get_emax_min(): bigint {
  return EMAX_MIN;
}
