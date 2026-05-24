/**
 * reference_ports/broken/mpfr_set_flt.ts — deliberately-buggy.
 *
 * Multi-bug: (1) passes 2*f instead of f, (2) negates the ternary on
 * the way out.
 *
 * NOT used in production.
 */

import type { Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_set_d } from '../../../src/ops/set_d.ts';

export function mpfr_set_flt(
  f: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  const r = mpfr_set_d(2 * f, prec, rnd);  // BUG: 2*f
  return { value: r.value, ternary: (-r.ternary) as Ternary };  // BUG: negate ternary
}
