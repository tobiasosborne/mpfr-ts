/**
 * reference_ports/correct/mpfr_clear_erangeflag.ts -- mutation-prove reference.
 * Algorithm (mpfr/src/exceptions.c L192-L198): mask & (63n ^ 16n) = mask & 47n.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_ERANGE = 16n;

export function mpfr_clear_erangeflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_erangeflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_ERANGE);
}
