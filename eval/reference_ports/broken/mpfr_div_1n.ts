/**
 * reference_ports/broken/mpfr_div_1n.ts — deliberately-buggy.
 *
 * Multi-bug: (1) returns the inverse (v/u not u/v), (2) shifts exp by -1.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div } from '../../../src/ops/div.ts';

export function mpfr_div_1n(u: MPFR, v: MPFR, rnd: RoundingMode): Result {
  const r = mpfr_div(v, u, 64n, rnd);  // BUG 1: swapped
  if (r.value.kind !== 'normal') return r;
  return { value: { ...r.value, exp: r.value.exp - 1n }, ternary: r.ternary };  // BUG 2: shift exp
}
