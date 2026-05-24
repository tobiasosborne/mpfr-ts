/**
 * reference_ports/correct/mpfr_max.ts — re-export of the production port.
 *
 * Ref: src/ops/max.ts — the algorithm.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_max as _impl } from '../../../src/ops/max.ts';

export function mpfr_max(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(a, b, prec, rnd);
}
