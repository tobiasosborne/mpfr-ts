/**
 * reference_ports/correct/mpfr_greater_p.ts — re-export of the production port.
 * See ./mpfr_less_p.ts for the layout rationale.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_greater_p as _impl } from '../../../src/ops/greater_p.ts';

export function mpfr_greater_p(a: MPFR, b: MPFR): boolean {
  return _impl(a, b);
}
