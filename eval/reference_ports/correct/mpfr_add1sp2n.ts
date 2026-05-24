/**
 * reference_ports/correct/mpfr_add1sp2n.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_add1sp2n as _impl } from '../../../src/ops/add1sp2n.ts';

export function mpfr_add1sp2n(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  return _impl(b, c, rnd);
}
