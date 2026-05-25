/**
 * reference_ports/broken/mpfr_check.ts -- deliberately-buggy.
 *
 * **BUG: returns false instead of true.** Trivially-wrong polarity;
 * every valid MPFR (which the test surface entirely consists of) fails.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_check(_x: _MPFR): boolean {
  // BUG: should return true for any valid MPFR (which is what the
  // golden tests). Always returning false fails the entire wire suite.
  return false;
}
