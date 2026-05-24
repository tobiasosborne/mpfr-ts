/**
 * reference_ports/correct/mpfr_mul_2ui.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_2ui as _impl } from '../../../src/ops/mul_2ui.ts';

export function mpfr_mul_2ui(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(x, n, prec, rnd);
}
