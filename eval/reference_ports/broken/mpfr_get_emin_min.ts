/**
 * reference_ports/broken/mpfr_get_emin_min.ts -- deliberately-buggy.
 *
 * **BUG: returns 0n instead of the actual EMIN_MIN constant.**
 * Collapse-to-constant per worklog 018 lesson. The single happy case
 * expects -4611686018427387903n; got 0n -> composite=0.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_get_emin_min(): bigint {
  // BUG: should return the EMIN_MIN constant.
  return 0n;
}
