/**
 * reference_ports/correct/mpfr_cmp_d.ts — re-export of the production port.
 *
 * See src/ops/cmp_d.ts for the algorithm.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp_d as _impl } from '../../../src/ops/cmp_d.ts';

export function mpfr_cmp_d(x: MPFR, d: number): number {
  return _impl(x, d);
}
