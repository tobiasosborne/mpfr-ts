/**
 * reference_ports/correct/mpfr_clear_inexflag.ts -- mutation-prove reference.
 * Algorithm (mpfr/src/exceptions.c L184-L190): mask & (63n ^ 8n) = mask & 55n.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_INEXACT = 8n;

export function mpfr_clear_inexflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_inexflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT);
}
