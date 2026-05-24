/**
 * reference_ports/correct/mpfr_sub1sp3.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_sub1sp3 as _impl } from '../../../src/ops/sub1sp3.ts';

export function mpfr_sub1sp3(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  return _impl(b, c, rnd);
}
