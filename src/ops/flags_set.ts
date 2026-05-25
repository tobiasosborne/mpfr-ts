/**
 * ops/flags_set.ts -- pure-TS port of MPFR's `mpfr_flags_set`.
 *
 * General mask-driven OR: take a desired prior flag state `pre` and a
 * bitmask `mask`, return the new flag state with the named bits set.
 * Mirrors the C `__gmpfr_flags |= mask` against a wire form where the
 * prior register state is passed explicitly.
 *
 * Algorithm (mpfr/src/exceptions.c L109-L115):
 *
 *   __gmpfr_flags |= mask
 *
 * Ref: mpfr/src/exceptions.c L109-L115 -- C reference body.
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants.
 * Ref: src/internal/mpfr/flags.ts -- setFlags(mask) is the canonical OR path.
 */

import { MPFRError } from '../core.ts';
import { clearFlags, setFlags, getFlags } from '../internal/mpfr/flags.ts';

/**
 * OR the bits in `mask` into `pre` and return the resulting register state.
 *
 * @mpfrName mpfr_flags_set
 *
 * @param pre   Pre-set flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off.
 * @param mask  Bits to set. Bits outside MPFR_FLAGS_ALL silently ignored.
 * @returns     `(pre | mask) & 63n` -- the post-OR register state.
 */
export function mpfr_flags_set(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_set: pre must be bigint, got ${typeof pre}`,
    );
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_set: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(pre);
  setFlags(mask);
  return getFlags();
}
