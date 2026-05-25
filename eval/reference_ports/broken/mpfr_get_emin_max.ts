/**
 * reference_ports/broken/mpfr_get_emin_max.ts -- deliberately-buggy.
 *
 * **BUG: returns 0n instead of EMIN_MAX.** Collapse-to-constant.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_get_emin_max(): bigint {
  return 0n;
}
