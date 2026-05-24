/**
 * reference_ports/correct/mpfr_add1sp1n.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_add1sp1n as _impl } from '../../../src/ops/add1sp1n.ts';

export function mpfr_add1sp1n(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  return _impl(b, c, rnd);
}
