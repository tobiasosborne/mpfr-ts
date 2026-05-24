/**
 * reference_ports/correct/mpfr_set_flt.ts — re-export of the production port.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_set_flt as _impl } from '../../../src/ops/set_flt.ts';

export function mpfr_set_flt(
  f: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(f, prec, rnd);
}
