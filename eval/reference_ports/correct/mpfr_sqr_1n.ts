/**
 * reference_ports/correct/mpfr_sqr_1n.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_sqr_1n as _impl } from '../../../src/ops/sqr_1n.ts';

export function mpfr_sqr_1n(b: MPFR, rnd: RoundingMode): Result {
  return _impl(b, rnd);
}
