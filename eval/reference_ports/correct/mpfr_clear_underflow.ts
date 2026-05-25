/**
 * reference_ports/correct/mpfr_clear_underflow.ts -- mutation-prove
 * reference for mpfr_clear_underflow.
 *
 * Algorithm (mpfr/src/exceptions.c L152-L158):
 *   new_flags = mask & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW)
 *
 * Bit values: UNDERFLOW=1, ALL=63. Clearing UNDERFLOW from `mask`:
 *   mask & (63n ^ 1n) = mask & 62n.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_UNDERFLOW = 1n;

export function mpfr_clear_underflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_underflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW);
}
