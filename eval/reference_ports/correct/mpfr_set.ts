/**
 * reference_ports/correct/mpfr_set.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_set as _impl } from '../../../src/ops/set.ts';

export function mpfr_set(b: MPFR, prec: bigint, rnd: RoundingMode): Result {
  return _impl(b, prec, rnd);
}
