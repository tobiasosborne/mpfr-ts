/**
 * reference_ports/broken/mpfr_ceil.ts — deliberately-buggy mpfr_ceil.
 *
 * **Deliberately broken: rounds toward -∞ instead of +∞ (floor instead
 * of ceil).** Symmetric counterpart to the broken floor.
 *
 * Behaviour:
 *   - NaN/Inf/Zero: matches correct.
 *   - 2.7 → 2 (correct says 3).
 *   - -2.7 → -3 (correct says -2).
 *   - 0.3 → +0 (correct says 1).
 *   - -0.3 → -1 (correct says -0).
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/ceil.ts — the correct version.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_floor as _floorImpl } from '../../../src/ops/floor.ts';

export function mpfr_ceil(x: MPFR, prec: bigint): Result {
  // BUG: should round toward +∞. Rounds toward -∞ (= floor) instead.
  return _floorImpl(x, prec);
}
