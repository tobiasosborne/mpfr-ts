/**
 * reference_ports/broken/mpfr_get_d1.ts — deliberately-buggy.
 *
 * Multi-bug: (1) uses RNDZ instead of the default RNDN, (2) negates
 * the result before returning.
 *
 * NOT used in production.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_get_d } from '../../../src/ops/get_d.ts';

export function mpfr_get_d1(x: MPFR): number {
  // BUG: wrong rnd; negated.
  return -mpfr_get_d(x, 'RNDZ');
}
