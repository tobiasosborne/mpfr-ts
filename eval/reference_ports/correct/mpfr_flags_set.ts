/**
 * reference_ports/correct/mpfr_flags_set.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L109-L115):
 *   __gmpfr_flags |= mask
 *
 * Port takes (pre, mask), returns (pre | mask) & MPFR_FLAGS_ALL.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_set(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_set: pre must be bigint, got ${typeof pre}`);
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_set: mask must be bigint, got ${typeof mask}`);
  }
  return (pre | mask) & MPFR_FLAGS_ALL;
}
