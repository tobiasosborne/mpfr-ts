/**
 * reference_ports/correct/mpfr_flags_restore.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L133-L141):
 *   __gmpfr_flags = (__gmpfr_flags & (MPFR_FLAGS_ALL ^ mask)) | (flags & mask)
 *
 * Replace mask-named bits of pre with the corresponding bits of flags;
 * bits NOT in mask are preserved from pre.
 *
 * Port takes (pre, flags, mask), returns ((pre & ~mask) | (flags & mask)) & ALL.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_restore(pre: bigint, flags: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_restore: pre must be bigint`);
  }
  if (typeof flags !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_restore: flags must be bigint`);
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_restore: mask must be bigint`);
  }
  const inPre = pre & MPFR_FLAGS_ALL;
  const inFlags = flags & MPFR_FLAGS_ALL;
  const inMask = mask & MPFR_FLAGS_ALL;
  return ((inPre & (MPFR_FLAGS_ALL ^ inMask)) | (inFlags & inMask)) & MPFR_FLAGS_ALL;
}
