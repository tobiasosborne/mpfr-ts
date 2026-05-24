/**
 * reference_ports/correct/mpfr_div_1n.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div_1n as _impl } from '../../../src/ops/div_1n.ts';

export function mpfr_div_1n(u: MPFR, v: MPFR, rnd: RoundingMode): Result {
  return _impl(u, v, rnd);
}
