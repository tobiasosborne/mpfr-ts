/**
 * reference_ports/broken/mpfr_mul_1.ts — deliberately-buggy.
 *
 * Multi-bug: (1) uses b.sign only (ignores product-of-signs rule),
 * (2) swaps operand order — calls mpfr_mul(c, b) which is fine but
 * we additionally negate ternary.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { mpfr_mul } from '../../../src/ops/mul.ts';

export function mpfr_mul_1(
  b: MPFR, c: MPFR, prec: bigint, rnd: RoundingMode,
): Result {
  const r = mpfr_mul(b, c, prec, rnd);
  if (r.value.kind !== 'normal' && r.value.kind !== 'zero') return r;
  // BUG 1: force sign to b.sign (wrong for mixed signs).
  const v = r.value;
  const wrong: MPFR = { kind: v.kind, sign: b.sign as Sign, prec: v.prec, exp: v.exp, mant: v.mant };
  // BUG 2: negate ternary.
  return { value: wrong, ternary: (-r.ternary) as Ternary };
}
