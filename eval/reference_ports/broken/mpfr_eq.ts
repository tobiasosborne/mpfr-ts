/**
 * reference_ports/broken/mpfr_eq.ts -- deliberately-buggy.
 *
 * **BUG: always returns true.** Strongest perturbation: collapses the
 * entire decision tree to a constant. Every case where libmpfr returns
 * false fails. Strengthened from the earlier NaN==NaN + harmless-off-by-one
 * variant (which scored 0.95 — the NaN==NaN bug alone hits too few cases,
 * and the exponent off-by-one was algebraically harmless so didn't add
 * mutation pressure).
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_eq(_u: MPFR, _v: MPFR, n_bits: bigint): boolean {
  if (typeof n_bits !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_eq: n_bits must be bigint`);
  }
  // BUG: ignores both operands; returns true.
  return true;
}
