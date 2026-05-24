/**
 * reference_ports/correct/mpfr_div_2exp.ts — re-export of the production port.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div_2exp as _impl } from '../../../src/ops/div_2exp.ts';

export function mpfr_div_2exp(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(x, n, prec, rnd);
}
