/**
 * reference_ports/broken/mpfr_get_d_2exp.ts — deliberately-buggy.
 *
 * Multi-bug: (1) returns the value WITHOUT scaling to [0.5, 1.0)
 * (returns it as-is via get_d), (2) returns exp=0 always.
 *
 * NOT used in production.
 */

import type { MPFR, RoundingMode } from '../../../src/core.ts';
import { mpfr_get_d } from '../../../src/ops/get_d.ts';

export function mpfr_get_d_2exp(
  x: MPFR,
  rnd: RoundingMode,
): { value: number; exp: bigint } {
  // BUG: no mantissa scaling; exp always 0.
  return { value: mpfr_get_d(x, rnd), exp: 0n };
}
