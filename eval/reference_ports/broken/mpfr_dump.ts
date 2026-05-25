/**
 * reference_ports/broken/mpfr_dump.ts -- deliberately-buggy.
 *
 * **BUG: returns empty string for everything.** Collapse-to-constant.
 * Every case (which expects a non-empty formatted dump) fails.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_dump(x: MPFR): string {
  void x;
  return '';
}
