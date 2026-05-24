/**
 * reference_ports/broken/mpfr_div_2exp.ts — deliberately-buggy mpfr_div_2exp.
 *
 * Multi-bug: (1) delegates to mpfr_mul_2ui (wrong direction), (2) passes
 * n+1 (off-by-one).
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_2ui } from '../../../src/ops/mul_2ui.ts';

export function mpfr_div_2exp(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // BUG: should div_2ui, not mul_2ui; n+1 not n.
  return mpfr_mul_2ui(x, n + 1n, prec, rnd);
}
