/**
 * reference_ports/broken/mpfr_nexttoward.ts -- deliberately-buggy.
 *
 * **Collapses the entire decision tree to a constant output** (per
 * HANDOFF gotcha #10): every input returns NAN_VALUE. Every non-NaN
 * input case fails on strict equality; composite well below 0.30.
 */

import type { MPFR } from '../../../src/core.ts';
import { NAN_VALUE } from '../../../src/core.ts';

export function mpfr_nexttoward(_x: MPFR, _y: MPFR): MPFR {
  // BUG: ignore inputs entirely; always return NaN.
  return NAN_VALUE;
}
