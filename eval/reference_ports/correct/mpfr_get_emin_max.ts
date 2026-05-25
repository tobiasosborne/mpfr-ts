/**
 * reference_ports/correct/mpfr_get_emin_max.ts -- mutation-prove reference.
 *
 * MPFR_EMIN_MAX = MPFR_EXP_INVALID - 1 = +(2^62 - 1) on 64-bit.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

const EMIN_MAX = (1n << 62n) - 1n;  // +4611686018427387903n

export function mpfr_get_emin_max(): bigint {
  return EMIN_MAX;
}
