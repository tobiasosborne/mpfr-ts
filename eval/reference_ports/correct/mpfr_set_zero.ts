/**
 * reference_ports/correct/mpfr_set_zero.ts — re-export of the production port.
 *
 * See reference_ports/correct/mpfr_set_nan.ts for the layout rationale.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Sign } from '../../../src/core.ts';
import { mpfr_set_zero as _impl } from '../../../src/ops/set_zero.ts';

export function mpfr_set_zero(prec: bigint, sign: Sign): MPFR {
  return _impl(prec, sign);
}
