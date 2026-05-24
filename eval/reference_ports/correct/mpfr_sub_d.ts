/**
 * reference_ports/correct/mpfr_sub_d.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_sub_d as _impl } from '../../../src/ops/sub_d.ts';

export function mpfr_sub_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
