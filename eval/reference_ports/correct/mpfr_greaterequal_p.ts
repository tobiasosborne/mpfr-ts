/**
 * reference_ports/correct/mpfr_greaterequal_p.ts — re-export of the production port.
 * See ./mpfr_less_p.ts for the layout rationale.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_greaterequal_p as _impl } from '../../../src/ops/greaterequal_p.ts';

export function mpfr_greaterequal_p(a: MPFR, b: MPFR): boolean {
  return _impl(a, b);
}
