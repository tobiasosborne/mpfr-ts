/**
 * ops/flags_test.ts -- pure-TS port of MPFR's `mpfr_flags_test`.
 *
 * General mask-driven read: take a desired prior flag state `pre` and a
 * bitmask `mask`, return the bits of `pre` that overlap `mask`. Mirrors
 * the C `return __gmpfr_flags & mask` against a wire form where the
 * prior register state is passed explicitly.
 *
 * Algorithm (mpfr/src/exceptions.c L117-L123):
 *
 *   return __gmpfr_flags & mask
 *
 * Ref: mpfr/src/exceptions.c L117-L123 -- C reference body.
 * Ref: src/internal/mpfr/flags.ts -- getFlags() & mask is the canonical read.
 */

import { MPFRError } from '../core.ts';
import { clearFlags, setFlags, getFlags } from '../internal/mpfr/flags.ts';

/**
 * Return the bits of `pre` that overlap `mask`.
 *
 * @mpfrName mpfr_flags_test
 *
 * @param pre   Prior flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off.
 * @param mask  Bits to test. Bits outside MPFR_FLAGS_ALL silently ignored.
 * @returns     `(pre & mask) & 63n`.
 */
export function mpfr_flags_test(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_test: pre must be bigint, got ${typeof pre}`,
    );
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_test: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(pre);
  return getFlags() & mask;
}
