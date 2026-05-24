/**
 * reference_ports/correct/mpfr_abs.ts — re-export of the production port.
 *
 * Symmetric to `correct/mpfr_neg.ts`; see that file's commentary for the
 * directory layout rationale.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: src/ops/abs.ts — the algorithm.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_abs as _impl } from '../../../src/ops/abs.ts';

export function mpfr_abs(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  return _impl(x, prec, rnd);
}
