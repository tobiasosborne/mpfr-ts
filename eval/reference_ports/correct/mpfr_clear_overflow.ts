/**
 * reference_ports/correct/mpfr_clear_overflow.ts -- mutation-prove
 * reference for mpfr_clear_overflow.
 *
 * Algorithm (mpfr/src/exceptions.c L160-L166):
 *   new_flags = mask & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW)
 *
 * Bit: OVERFLOW=2, ALL=63. Clearing: mask & (63n ^ 2n) = mask & 61n.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_OVERFLOW = 2n;

export function mpfr_clear_overflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_overflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW);
}
