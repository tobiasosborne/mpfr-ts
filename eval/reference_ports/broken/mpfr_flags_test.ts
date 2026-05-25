/**
 * reference_ports/broken/mpfr_flags_test.ts -- deliberately-buggy.
 *
 * **BUG: returns the OR (union) instead of AND (intersection).**
 * Treats test like set; every case where pre and mask differ on any
 * bit will yield a wrong result.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_test(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint' || typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_test: bad input`);
  }
  // BUG: should be AND; OR is set semantics, not test semantics.
  return (pre | mask) & MPFR_FLAGS_ALL;
}
