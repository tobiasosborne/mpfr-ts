/**
 * reference_ports/correct/mpfr_inf_p.ts — re-export of the production port.
 * See ./mpfr_nan_p.ts for the layout rationale.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_inf_p as _impl } from '../../../src/ops/inf_p.ts';

export function mpfr_inf_p(x: MPFR): boolean {
  return _impl(x);
}
