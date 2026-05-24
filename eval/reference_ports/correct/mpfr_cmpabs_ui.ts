/**
 * reference_ports/correct/mpfr_cmpabs_ui.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmpabs_ui as _impl } from '../../../src/ops/cmpabs_ui.ts';

export function mpfr_cmpabs_ui(b: MPFR, c: bigint): number {
  return _impl(b, c);
}
