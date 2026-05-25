/**
 * reference_ports/broken/mpfr_clear_nanflag.ts -- deliberately-buggy.
 * **BUG: clears ERANGE (16) instead of NAN (4).** Off-by-position bug.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_ERANGE = 16n;  /* BUG: should be NAN (4) */

export function mpfr_clear_nanflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_nanflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  // BUG: wrong bit (ERANGE=16, should be NAN=4).
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_ERANGE);
}
