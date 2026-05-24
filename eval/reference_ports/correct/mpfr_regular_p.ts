/**
 * reference_ports/correct/mpfr_regular_p.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_regular_p as _impl } from '../../../src/ops/regular_p.ts';

export function mpfr_regular_p(x: MPFR): boolean {
  return _impl(x);
}
