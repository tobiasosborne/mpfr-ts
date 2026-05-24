/**
 * reference_ports/correct/mpfr_get_d1.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_get_d1 as _impl } from '../../../src/ops/get_d1.ts';

export function mpfr_get_d1(x: MPFR): number {
  return _impl(x);
}
