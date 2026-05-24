/**
 * reference_ports/correct/mpfr_unordered_p.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_unordered_p as _impl } from '../../../src/ops/unordered_p.ts';

export function mpfr_unordered_p(a: MPFR, b: MPFR): boolean {
  return _impl(a, b);
}
