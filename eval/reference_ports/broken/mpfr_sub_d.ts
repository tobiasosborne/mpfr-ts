/**
 * reference_ports/broken/mpfr_sub_d.ts — deliberately-buggy mpfr_sub_d.
 *
 * Multi-bug: (1) calls mpfr_add (wrong op), (2) negates ternary on the way
 * out. Every case fails on value AND ternary direction.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_add } from '../../../src/ops/add.ts';
import { mpfr_set_d } from '../../../src/ops/set_d.ts';

export function mpfr_sub_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  const dMpfr = mpfr_set_d(c, 53n, rnd).value;
  const r = mpfr_add(b, dMpfr, prec, rnd);  // BUG: add not sub
  return { value: r.value, ternary: (-r.ternary) as Ternary };  // BUG: negate ternary
}
