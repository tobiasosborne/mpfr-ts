/**
 * reference_ports/correct/mpfr_div_d.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div_d as _impl } from '../../../src/ops/div_d.ts';

export function mpfr_div_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
