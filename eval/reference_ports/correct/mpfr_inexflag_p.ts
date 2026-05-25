/**
 * reference_ports/correct/mpfr_inexflag_p.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L366-L373):
 *   return __gmpfr_flags & MPFR_FLAGS_INEXACT
 *
 * Returns the INEXACT bit (8) of the global flag register as a boolean
 * (per ADR 0001 idiomatic-TS lift). Structural twin of erangeflag_p.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_INEXACT = 8n;

export function mpfr_inexflag_p(mask: bigint): boolean {
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_inexflag_p: mask must be bigint`);
  }
  return (mask & MPFR_FLAGS_INEXACT) !== 0n;
}
