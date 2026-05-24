/**
 * reference_ports/correct/mpfr_odd_p.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_odd_p as _impl } from '../../../src/ops/odd_p.ts';

export function mpfr_odd_p(x: MPFR): boolean {
  return _impl(x);
}
