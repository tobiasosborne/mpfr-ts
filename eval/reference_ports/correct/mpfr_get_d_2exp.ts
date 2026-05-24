/**
 * reference_ports/correct/mpfr_get_d_2exp.ts — re-export of the production port.
 */

import type { MPFR, RoundingMode } from '../../../src/core.ts';
import { mpfr_get_d_2exp as _impl } from '../../../src/ops/get_d_2exp.ts';

export function mpfr_get_d_2exp(
  x: MPFR,
  rnd: RoundingMode,
): { value: number; exp: bigint } {
  return _impl(x, rnd);
}
