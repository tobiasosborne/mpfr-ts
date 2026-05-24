/**
 * reference_ports/correct/mpfr_mul_1n.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_1n as _impl } from '../../../src/ops/mul_1n.ts';

export function mpfr_mul_1n(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  return _impl(b, c, rnd);
}
