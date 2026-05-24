/**
 * reference_ports/broken/mpfr_get_exp.ts — deliberately-buggy mpfr_get_exp.
 *
 * **Multi-bug perturbation:**
 *   1. For kind='normal', returns x.exp - 1n instead of x.exp. Off-by-
 *      one error, every case fails.
 *   2. For non-normal kinds, returns 0n instead of throwing. The golden
 *      contains only normal inputs (per the driver design), so this bug
 *      doesn't manifest in cases — but it's a real bug a careless port
 *      would introduce.
 *
 * NOT used in production.
 *
 * Ref: src/ops/get_exp.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_get_exp(x: MPFR): bigint {
  // BUG 2 (latent): returns 0 instead of throwing for non-normals.
  if (x.kind !== 'normal') return 0n;
  // BUG 1: off-by-one.
  return x.exp - 1n;
}
