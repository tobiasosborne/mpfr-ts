/**
 * reference_ports/correct/mpfr_min.ts — re-export of the production port.
 *
 * Ref: src/ops/min.ts — the algorithm.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_min as _impl } from '../../../src/ops/min.ts';

export function mpfr_min(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(a, b, prec, rnd);
}
