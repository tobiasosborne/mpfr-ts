/**
 * reference_ports/broken/mpfr_cmpabs.ts — deliberately-buggy mpfr_cmpabs.
 *
 * Multi-bug: (1) delegates to mpfr_cmp WITHOUT taking absolute value
 * first (so signed comparison rather than magnitude comparison),
 * (2) negates the result (sign flip).
 *
 * NOT used in production.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp } from '../../../src/ops/cmp.ts';

export function mpfr_cmpabs(b: MPFR, c: MPFR): number {
  // BUG 1: pass through signs; BUG 2: negate the result.
  return -mpfr_cmp(b, c);
}
