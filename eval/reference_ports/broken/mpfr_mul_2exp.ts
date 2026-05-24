/**
 * reference_ports/broken/mpfr_mul_2exp.ts — deliberately-buggy mpfr_mul_2exp.
 *
 * Multi-bug: (1) delegates to mpfr_div_2ui (wrong direction), (2) passes
 * n+1 instead of n (off-by-one). Almost every case fails.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div_2ui } from '../../../src/ops/div_2ui.ts';

export function mpfr_mul_2exp(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // BUG: should mul_2ui, not div_2ui; n+1 not n.
  return mpfr_div_2ui(x, n + 1n, prec, rnd);
}
