/**
 * reference_ports/broken/mpfr_mul_1n.ts — deliberately-buggy.
 *
 * Multi-bug: (1) flips one operand's sign (always produces wrong-sign
 * result), (2) bumps the exponent by 1 (doubles the value magnitude),
 * (3) returns ternary=0 unconditionally (claims exact for everything).
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul } from '../../../src/ops/mul.ts';

export function mpfr_mul_1n(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  const bFlip: MPFR = b.kind === 'normal'
    ? { ...b, sign: (b.sign === 1 ? -1 : 1) as 1 | -1 }
    : b;
  const r = mpfr_mul(bFlip, c, 64n, rnd);
  const value: MPFR = r.value.kind === 'normal'
    ? { ...r.value, exp: r.value.exp + 1n }
    : r.value;
  return { value, ternary: 0 };
}
