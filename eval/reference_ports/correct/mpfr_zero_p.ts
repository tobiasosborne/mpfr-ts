/**
 * reference_ports/correct/mpfr_zero_p.ts — re-export of the production port.
 * See ./mpfr_nan_p.ts for the layout rationale.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_zero_p as _impl } from '../../../src/ops/zero_p.ts';

export function mpfr_zero_p(x: MPFR): boolean {
  return _impl(x);
}
