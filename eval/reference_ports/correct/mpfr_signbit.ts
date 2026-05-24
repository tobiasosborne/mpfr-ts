/**
 * reference_ports/correct/mpfr_signbit.ts — re-export of the production port.
 * See ./mpfr_nan_p.ts for the layout rationale.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_signbit as _impl } from '../../../src/ops/signbit.ts';

export function mpfr_signbit(x: MPFR): boolean {
  return _impl(x);
}
