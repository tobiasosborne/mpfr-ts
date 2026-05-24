/**
 * reference_ports/correct/mpfr_cmp_ui.ts — re-export of the production port.
 *
 * See src/ops/cmp_ui.ts for the algorithm.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp_ui as _impl } from '../../../src/ops/cmp_ui.ts';

export function mpfr_cmp_ui(x: MPFR, n: bigint): number {
  return _impl(x, n);
}
