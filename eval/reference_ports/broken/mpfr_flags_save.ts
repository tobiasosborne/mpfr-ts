/**
 * reference_ports/broken/mpfr_flags_save.ts -- deliberately-buggy.
 *
 * **BUG: returns the bitwise-complement (~pre) & ALL instead of pre & ALL.**
 * Inverts every bit. Every nonzero pre yields a wrong result.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_save(pre: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_save: bad input`);
  }
  // BUG: invert; should return pre & ALL.
  return (~pre) & MPFR_FLAGS_ALL;
}
