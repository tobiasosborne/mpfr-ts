/**
 * reference_ports/broken/mpfr_d_div.ts — deliberately-buggy mpfr_d_div.
 *
 * Multi-bug: (1) reverses argument order — c/b instead of b/c,
 * (2) inverts the rounding mode. Both errors compound.
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

export function mpfr_d_div(
  b: number,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  const inv = invertRnd(rnd);  // BUG 2: invert rnd.
  const bMpfr = mpfr_set_d(b, 53n, inv).value;
  // BUG 1: swap arguments — c/bMpfr instead of bMpfr/c.
  return mpfr_div(c, bMpfr, prec, inv);
}
