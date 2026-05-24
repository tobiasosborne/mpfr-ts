/**
 * reference_ports/correct/mpfr_cmp3.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp3 as _impl } from '../../../src/ops/cmp3.ts';

export function mpfr_cmp3(b: MPFR, c: MPFR, s: number): number {
  return _impl(b, c, s);
}
