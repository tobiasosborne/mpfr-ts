/**
 * reference_ports/broken/mpfr_clear_inexflag.ts -- deliberately-buggy.
 * **BUG: clears DIVBY0 (32) instead of INEXACT (8).**
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_DIVBY0 = 32n;  /* BUG: should be INEXACT (8) */

export function mpfr_clear_inexflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_inexflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  // BUG: wrong bit (DIVBY0=32, should be INEXACT=8).
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);
}
