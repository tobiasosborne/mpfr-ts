/**
 * reference_ports/broken/mpfr_get_emax_max.ts -- deliberately-buggy.
 *
 * **BUG: returns 0n instead of EMAX_MAX.** Collapse-to-constant.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_get_emax_max(): bigint {
  return 0n;
}
