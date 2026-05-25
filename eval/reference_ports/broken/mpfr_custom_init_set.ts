/**
 * reference_ports/broken/mpfr_custom_init_set.ts -- deliberately-buggy.
 *
 * **BUG: always returns NaN regardless of kind.** Strongest perturbation
 * that bypasses the entire kind-decode branch tree. Every non-NaN case
 * fails on the wire. Strengthened from earlier swap-INF-ZERO variant
 * (which only hit ~37% of cases); the new variant hits every case where
 * the expected output is not NaN.
 */

import type { MPFR } from '../../../src/core.ts';
import { NAN_VALUE } from '../../../src/core.ts';

export function mpfr_custom_init_set(
  _kind: number,
  _exp: bigint,
  _prec: bigint,
  _mantissa: bigint,
): MPFR {
  // BUG: collapses the kind-decode tree to a constant NaN.
  return NAN_VALUE;
}
