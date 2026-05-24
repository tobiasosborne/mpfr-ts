/**
 * reference_ports/broken/mpfr_mul_d.ts — deliberately-buggy mpfr_mul_d.
 *
 * Multi-bug: (1) calls mpfr_div instead of mpfr_mul (very wrong op),
 * (2) inverts rounding mode (RNDU↔RNDD, others unchanged). Every case
 * fails on value AND most on ternary direction.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div } from '../../../src/ops/div.ts';
import { mpfr_set_d } from '../../../src/ops/set_d.ts';

function invertRnd(rnd: RoundingMode): RoundingMode {
  if (rnd === 'RNDU') return 'RNDD';
  if (rnd === 'RNDD') return 'RNDU';
  return rnd;
}

export function mpfr_mul_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  const inv = invertRnd(rnd);  // BUG: invert rnd
  const dMpfr = mpfr_set_d(c, 53n, inv).value;
  return mpfr_div(b, dMpfr, prec, inv);  // BUG: div not mul
}
