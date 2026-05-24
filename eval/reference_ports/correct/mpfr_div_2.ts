/**
 * reference_ports/correct/mpfr_div_2.ts -- re-export of the production port.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div_2 as _impl } from '../../../src/ops/div_2.ts';

export function mpfr_div_2(
  u: MPFR, v: MPFR, prec: bigint, rnd: RoundingMode,
): Result {
  return _impl(u, v, prec, rnd);
}
