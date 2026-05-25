/**
 * reference_ports/broken/mpfr_clear_underflow.ts -- deliberately-buggy.
 *
 * **BUG: clears MPFR_FLAGS_NAN (bit 4) instead of MPFR_FLAGS_UNDERFLOW
 * (bit 1).** Common off-by-position bug; visible when mask has NAN set
 * but not UNDERFLOW, or vice versa.
 *
 * Expected gap: correct=1.0, broken<0.55. Mismatches the majority of
 * fuzz/edge cases.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_NAN = 4n;  /* BUG: should be UNDERFLOW (1) */

export function mpfr_clear_underflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_underflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  // BUG: clears NAN bit (4) instead of UNDERFLOW (1).
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN);
}
