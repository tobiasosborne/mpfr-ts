/**
 * reference_ports/correct/mpfr_flags_save.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L125-L131):
 *   return __gmpfr_flags
 *
 * Port takes (pre), returns pre & MPFR_FLAGS_ALL.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_save(pre: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_save: pre must be bigint, got ${typeof pre}`);
  }
  return pre & MPFR_FLAGS_ALL;
}
