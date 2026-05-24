/**
 * reference_ports/broken/mpfr_div_1.ts — deliberately-buggy.
 *
 * Multi-bug: (1) swaps operands (computes v/u), (2) inverts rnd.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div } from '../../../src/ops/div.ts';

function invertRnd(rnd: RoundingMode): RoundingMode {
  if (rnd === 'RNDU') return 'RNDD';
  if (rnd === 'RNDD') return 'RNDU';
  return rnd;
}

export function mpfr_div_1(
  u: MPFR, v: MPFR, prec: bigint, rnd: RoundingMode,
): Result {
  // BUG: swap operands; invert rnd.
  return mpfr_div(v, u, prec, invertRnd(rnd));
}
