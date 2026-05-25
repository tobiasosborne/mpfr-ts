/**
 * reference_ports/correct/mpfr_erangeflag_p.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L377-L382):
 *   return __gmpfr_flags & MPFR_FLAGS_ERANGE
 *
 * Returns the ERANGE bit (16) of the global flag register as a boolean
 * (per ADR 0001 idiomatic-TS lift).
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ERANGE = 16n;

export function mpfr_erangeflag_p(mask: bigint): boolean {
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_erangeflag_p: mask must be bigint`);
  }
  return (mask & MPFR_FLAGS_ERANGE) !== 0n;
}
