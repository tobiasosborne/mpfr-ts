/**
 * reference_ports/correct/mpfr_mul_1.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_1 as _impl } from '../../../src/ops/mul_1.ts';

export function mpfr_mul_1(
  b: MPFR, c: MPFR, prec: bigint, rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
