/**
 * reference_ports/correct/mpfr_d_div.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_d_div as _impl } from '../../../src/ops/d_div.ts';

export function mpfr_d_div(
  b: number,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
