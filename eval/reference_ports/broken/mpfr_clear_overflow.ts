/**
 * reference_ports/broken/mpfr_clear_overflow.ts -- deliberately-buggy.
 *
 * **BUG: clears MPFR_FLAGS_INEXACT (8) instead of MPFR_FLAGS_OVERFLOW
 * (2).** Off-by-position on the bit constant.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_INEXACT = 8n;  /* BUG: should be OVERFLOW (2) */

export function mpfr_clear_overflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_overflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  // BUG: wrong bit (INEXACT=8, should be OVERFLOW=2).
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT);
}
