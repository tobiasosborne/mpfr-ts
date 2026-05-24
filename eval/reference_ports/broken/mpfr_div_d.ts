/**
 * reference_ports/broken/mpfr_div_d.ts — deliberately-buggy mpfr_div_d.
 *
 * Multi-bug: (1) swaps the argument order so it computes c/b instead of
 * b/c, (2) negates ternary. Every non-self-inverse case fails.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_div } from '../../../src/ops/div.ts';
import { mpfr_set_d } from '../../../src/ops/set_d.ts';

export function mpfr_div_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  const dMpfr = mpfr_set_d(c, 53n, rnd).value;
  // BUG 1: argument swap — computes c/b instead of b/c.
  const r = mpfr_div(dMpfr, b, prec, rnd);
  // BUG 2: negate ternary.
  return { value: r.value, ternary: (-r.ternary) as Ternary };
}
