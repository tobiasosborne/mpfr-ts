/**
 * reference_ports/correct/mpfr_sub.ts — re-export of the production port.
 *
 * See reference_ports/correct/mpfr_set_nan.ts for the layout rationale.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_sub as _impl } from '../../../src/ops/sub.ts';

export function mpfr_sub(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(a, b, prec, rnd);
}
