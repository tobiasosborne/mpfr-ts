/**
 * reference_ports/broken/mpfr_get_emax_min.ts -- deliberately-buggy.
 *
 * **BUG: returns 0n instead of EMAX_MIN.** Collapse-to-constant.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_get_emax_min(): bigint {
  return 0n;
}
