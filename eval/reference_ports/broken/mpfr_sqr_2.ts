/**
 * reference_ports/broken/mpfr_sqr_2.ts — deliberately-buggy.
 *
 * Multi-bug: (1) inverts rounding mode, (2) doubles result exponent
 * (so result is too big by 2x).
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul } from '../../../src/ops/mul.ts';

function invertRnd(rnd: RoundingMode): RoundingMode {
  if (rnd === 'RNDU') return 'RNDD';
  if (rnd === 'RNDD') return 'RNDU';
  return rnd;
}

export function mpfr_sqr_2(
  b: MPFR, prec: bigint, rnd: RoundingMode,
): Result {
  const inv = invertRnd(rnd);  // BUG 1
  const r = mpfr_mul(b, b, prec, inv);
  if (r.value.kind !== 'normal') return r;
  // BUG 2: bump exponent.
  const v = r.value;
  return { value: { ...v, exp: v.exp + 1n }, ternary: r.ternary };
}
