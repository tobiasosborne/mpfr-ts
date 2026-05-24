/**
 * reference_ports/broken/mpfr_sqr_1.ts — deliberately-buggy.
 *
 * Multi-bug: (1) delegates to mpfr_mul with two different operands
 * (b and b+1ulp via a fudged second arg), (2) negates ternary.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_mul } from '../../../src/ops/mul.ts';

export function mpfr_sqr_1(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // BUG 1: multiply b by a different value (b with mantissa +1 — slight perturbation).
  if (b.kind !== 'normal') {
    return mpfr_mul(b, b, prec, rnd);
  }
  const c: MPFR = {
    kind: 'normal', sign: b.sign, prec: b.prec, exp: b.exp,
    mant: b.mant + 1n <= (1n << b.prec) - 1n ? b.mant + 1n : b.mant - 1n,
  };
  const r = mpfr_mul(b, c, prec, rnd);
  // BUG 2: negate ternary.
  return { value: r.value, ternary: (-r.ternary) as Ternary };
}
