/**
 * reference_ports/correct/mpfr_dim.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_dim as _impl } from '../../../src/ops/dim.ts';

export function mpfr_dim(
  b: MPFR,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
