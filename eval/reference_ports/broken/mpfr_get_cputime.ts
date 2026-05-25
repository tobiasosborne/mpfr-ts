/**
 * reference_ports/broken/mpfr_get_cputime.ts -- deliberately-buggy.
 *
 * **BUG: returns 1 instead of 0.** Single-case golden fails.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_get_cputime(): number {
  return 1;
}
