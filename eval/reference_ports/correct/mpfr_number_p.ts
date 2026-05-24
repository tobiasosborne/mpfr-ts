/**
 * reference_ports/correct/mpfr_number_p.ts — re-export of the production port.
 * See ./mpfr_nan_p.ts for the layout rationale.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_number_p as _impl } from '../../../src/ops/number_p.ts';

export function mpfr_number_p(x: MPFR): boolean {
  return _impl(x);
}
