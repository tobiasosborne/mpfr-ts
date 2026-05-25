/**
 * reference_ports/broken/mpfr_flags_clear.ts -- deliberately-buggy.
 *
 * **BUG: returns the pre state unchanged (mask is ignored).** Treats
 * the function as a no-op. Most cases differ from expected because the
 * golden exercises (pre, mask) pairs where mask actually clears bits.
 *
 * Expected gap: correct=1.0, broken well below 0.30.
 *
 * NOT used in production. Do NOT fix this file.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_clear(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_clear: pre must be bigint`);
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_clear: mask must be bigint`);
  }
  // BUG: ignore mask entirely; return pre unchanged.
  void mask;
  return pre & MPFR_FLAGS_ALL;
}
