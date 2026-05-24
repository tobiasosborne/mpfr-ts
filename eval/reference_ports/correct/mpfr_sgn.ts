/**
 * reference_ports/correct/mpfr_sgn.ts — re-export of the production port.
 *
 * Symmetric to `correct/mpfr_neg.ts`.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: src/ops/sgn.ts — the algorithm.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_sgn as _impl } from '../../../src/ops/sgn.ts';

export function mpfr_sgn(x: MPFR): number {
  return _impl(x);
}
