/**
 * reference_ports/correct/mpfr_flags_test.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L117-L123):
 *   return __gmpfr_flags & mask
 *
 * Port takes (pre, mask), returns (pre & mask) & MPFR_FLAGS_ALL.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_test(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_test: pre must be bigint, got ${typeof pre}`);
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_test: mask must be bigint, got ${typeof mask}`);
  }
  return (pre & mask) & MPFR_FLAGS_ALL;
}
