/**
 * reference_ports/correct/mpfr_clear_nanflag.ts -- mutation-prove reference.
 * Algorithm (mpfr/src/exceptions.c L176-L182):
 *   new_flags = mask & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN) = mask & 59n.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_NAN = 4n;

export function mpfr_clear_nanflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_nanflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN);
}
