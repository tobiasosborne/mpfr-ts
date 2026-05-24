/**
 * reference_ports/correct/mpfr_lessgreater_p.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_lessgreater_p as _impl } from '../../../src/ops/lessgreater_p.ts';

export function mpfr_lessgreater_p(a: MPFR, b: MPFR): boolean {
  return _impl(a, b);
}
