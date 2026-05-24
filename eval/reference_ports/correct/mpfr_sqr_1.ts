/**
 * reference_ports/correct/mpfr_sqr_1.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_sqr_1 as _impl } from '../../../src/ops/sqr_1.ts';

export function mpfr_sqr_1(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, prec, rnd);
}
