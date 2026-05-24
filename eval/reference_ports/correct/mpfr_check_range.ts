/**
 * reference_ports/correct/mpfr_check_range.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_check_range as _impl } from '../../../src/ops/check_range.ts';

export function mpfr_check_range(
  x: MPFR,
  t: Ternary,
  rnd: RoundingMode,
): Result {
  return _impl(x, t, rnd);
}
