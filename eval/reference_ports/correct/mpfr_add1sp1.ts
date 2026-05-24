/**
 * reference_ports/correct/mpfr_add1sp1.ts — re-export wrapper.
 *
 * Imports schema types for Law 4 compliance, delegates to the
 * production port. Do NOT duplicate.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_add1sp1 as _impl } from '../../../src/ops/add1sp1.ts';

export function mpfr_add1sp1(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  return _impl(b, c, rnd);
}
