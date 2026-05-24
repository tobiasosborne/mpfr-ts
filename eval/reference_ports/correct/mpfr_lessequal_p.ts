/**
 * reference_ports/correct/mpfr_lessequal_p.ts — re-export of the production port.
 * See ./mpfr_less_p.ts for the layout rationale.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_lessequal_p as _impl } from '../../../src/ops/lessequal_p.ts';

export function mpfr_lessequal_p(a: MPFR, b: MPFR): boolean {
  return _impl(a, b);
}
