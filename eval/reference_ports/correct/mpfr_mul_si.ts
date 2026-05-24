/**
 * reference_ports/correct/mpfr_mul_si.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_si as _impl } from '../../../src/ops/mul_si.ts';

export function mpfr_mul_si(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
