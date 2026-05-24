/**
 * reference_ports/correct/mpfr_mul_ui.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_ui as _impl } from '../../../src/ops/mul_ui.ts';

export function mpfr_mul_ui(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
