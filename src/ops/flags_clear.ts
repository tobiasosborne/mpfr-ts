/**
 * ops/flags_clear.ts -- pure-TS port of MPFR's `mpfr_flags_clear`.
 *
 * General mask-driven clear: take a desired prior flag state `pre` and a
 * bitmask `mask`, return the new flag state with the named bits cleared.
 * Mirrors the C `__gmpfr_flags &= MPFR_FLAGS_ALL ^ mask` against a wire
 * form where the prior register state is passed explicitly (consistent
 * with the rest of the flag family -- clear_divby0, clear_overflow, etc).
 *
 * Algorithm (mpfr/src/exceptions.c L101-L107):
 *
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ mask
 *
 * Ref: mpfr/src/exceptions.c L101-L107 -- C reference body.
 * Ref: mpfr/src/exceptions.c L37-L40 -- __gmpfr_flags declaration.
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants (ALL=63).
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../core.ts';
import { clearFlags, setFlags, getFlags } from '../internal/mpfr/flags.ts';

/**
 * Clear the bits named in `mask` from `pre` and return the resulting
 * register state.
 *
 * @mpfrName mpfr_flags_clear
 *
 * @param pre   Pre-clear flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off by the substrate.
 * @param mask  Bits to clear. Bits outside MPFR_FLAGS_ALL silently ignored.
 * @returns     Post-clear register state: `(pre & 63n) & (63n ^ (mask & 63n))`.
 */
export function mpfr_flags_clear(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_clear: pre must be bigint, got ${typeof pre}`,
    );
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_clear: mask must be bigint, got ${typeof mask}`,
    );
  }
  // Route through the shared register so flags.ts stays the single source
  // of truth for bit values and MPFR_FLAGS_ALL masking.
  clearFlags();
  setFlags(pre);
  clearFlags(mask);
  return getFlags();
}
