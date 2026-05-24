/**
 * reference_ports/correct/mpfr_mul_d.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_d as _impl } from '../../../src/ops/mul_d.ts';

export function mpfr_mul_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
