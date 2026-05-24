/**
 * reference_ports/broken/mpfr_floor.ts — deliberately-buggy mpfr_floor.
 *
 * **Deliberately broken: rounds toward +∞ instead of -∞ (ceil instead
 * of floor).** Mirrors a plausible agent error: "I swapped the RNDU and
 * RNDD branches when deriving from mpfr_rint."
 *
 * Behaviour:
 *   - NaN/Inf/Zero: matches correct.
 *   - 2.7 → 3 (correct says 2).
 *   - -2.7 → -2 (correct says -3).
 *   - 0.3 → 1 (correct says +0).
 *   - -0.3 → -0 (correct says -1).
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/floor.ts — the correct version.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_ceil as _ceilImpl } from '../../../src/ops/ceil.ts';

export function mpfr_floor(x: MPFR, prec: bigint): Result {
  // BUG: should round toward -∞. Rounds toward +∞ (= ceil) instead.
  return _ceilImpl(x, prec);
}
