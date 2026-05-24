/**
 * reference_ports/broken/mpfr_mul_2.ts — deliberately-buggy.
 *
 * Multi-bug: (1) bumps result exp by +1, (2) negates ternary.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_mul } from '../../../src/ops/mul.ts';

export function mpfr_mul_2(
  b: MPFR, c: MPFR, prec: bigint, rnd: RoundingMode,
): Result {
  const r = mpfr_mul(b, c, prec, rnd);
  if (r.value.kind !== 'normal') {
    return { value: r.value, ternary: (-r.ternary) as Ternary };
  }
  const v = r.value;
  return {
    value: { ...v, exp: v.exp + 1n },  // BUG 1: bump exp
    ternary: (-r.ternary) as Ternary,    // BUG 2: negate ternary
  };
}
