/**
 * reference_ports/correct/mpfr_set4.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../../../src/core.ts';
import { mpfr_set4 as _impl } from '../../../src/ops/set4.ts';

export function mpfr_set4(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
  signb: Sign,
): Result {
  return _impl(b, prec, rnd, signb);
}
