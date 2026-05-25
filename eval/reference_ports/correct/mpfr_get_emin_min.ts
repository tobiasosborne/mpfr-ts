/**
 * reference_ports/correct/mpfr_get_emin_min.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L52-L56):
 *   return MPFR_EMIN_MIN
 *
 * On the host's 64-bit mpfr_exp_t this evaluates to -(2^62 - 1) =
 * -4611686018427387903 = 1n - 2n**62n.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

const EMIN_MIN = 1n - (1n << 62n);  // -4611686018427387903n

export function mpfr_get_emin_min(): bigint {
  return EMIN_MIN;
}
