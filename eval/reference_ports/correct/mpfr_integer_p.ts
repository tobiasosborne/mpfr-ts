/**
 * reference_ports/correct/mpfr_integer_p.ts — re-export of the production port.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_integer_p as _impl } from '../../../src/ops/integer_p.ts';

export function mpfr_integer_p(x: MPFR): boolean {
  return _impl(x);
}
