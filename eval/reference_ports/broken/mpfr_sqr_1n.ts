/**
 * reference_ports/broken/mpfr_sqr_1n.ts — deliberately-buggy.
 *
 * Multi-bug: (1) flips the result sign to negative, (2) uses RNDZ
 * regardless of rnd argument.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../../../src/core.ts';
import { mpfr_mul } from '../../../src/ops/mul.ts';

export function mpfr_sqr_1n(b: MPFR, _rnd: RoundingMode): Result {
  const r = mpfr_mul(b, b, 64n, 'RNDZ');  // BUG: ignore rnd, use RNDZ
  if (r.value.kind !== 'normal' && r.value.kind !== 'zero') return r;
  // BUG: flip sign.
  const v = r.value;
  const flipped: MPFR = {
    kind: v.kind, sign: (v.sign === 1 ? -1 : 1) as Sign,
    prec: v.prec, exp: v.exp, mant: v.mant,
  };
  return { value: flipped, ternary: r.ternary };
}
