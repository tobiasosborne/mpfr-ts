/**
 * reference_ports/correct/mpfr_flags_clear.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L101-L107):
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ mask
 *
 * The port takes (pre, mask): pre is the desired prior flag state,
 * mask is the bits to clear. Returns the new flag state.
 *
 * Equivalent to calling setFlags(pre) then clearFlags(mask) from
 * src/internal/mpfr/flags.ts, then reading the register.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_clear(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_clear: pre must be bigint, got ${typeof pre}`);
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_clear: mask must be bigint, got ${typeof mask}`);
  }
  const inPre = pre & MPFR_FLAGS_ALL;
  const inMask = mask & MPFR_FLAGS_ALL;
  // C: register &= ALL ^ mask. With pre = current register, this is
  // pre & (ALL ^ mask).
  return inPre & (MPFR_FLAGS_ALL ^ inMask);
}
