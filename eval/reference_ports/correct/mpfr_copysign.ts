/**
 * reference_ports/correct/mpfr_copysign.ts — re-export of the production port.
 *
 * Ref: src/ops/copysign.ts — the algorithm.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_copysign as _impl } from '../../../src/ops/copysign.ts';

export function mpfr_copysign(
  x: MPFR,
  y: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(x, y, prec, rnd);
}
