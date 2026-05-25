/**
 * reference_ports/broken/mpfr_clear_erangeflag.ts -- deliberately-buggy.
 * **BUG: clears UNDERFLOW (1) instead of ERANGE (16).**
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_UNDERFLOW = 1n;  /* BUG: should be ERANGE (16) */

export function mpfr_clear_erangeflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_erangeflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  // BUG: wrong bit (UNDERFLOW=1, should be ERANGE=16).
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW);
}
