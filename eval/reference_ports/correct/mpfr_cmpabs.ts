/**
 * reference_ports/correct/mpfr_cmpabs.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmpabs as _impl } from '../../../src/ops/cmpabs.ts';

export function mpfr_cmpabs(b: MPFR, c: MPFR): number {
  return _impl(b, c);
}
