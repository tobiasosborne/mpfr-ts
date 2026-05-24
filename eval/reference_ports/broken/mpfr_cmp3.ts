/**
 * reference_ports/broken/mpfr_cmp3.ts — deliberately-buggy mpfr_cmp3.
 *
 * Multi-bug: (1) ignores s entirely, (2) returns -result.
 *
 * NOT used in production.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp } from '../../../src/ops/cmp.ts';

export function mpfr_cmp3(b: MPFR, c: MPFR, _s: number): number {
  return -mpfr_cmp(b, c);  // BUG: ignores s; negates result.
}
