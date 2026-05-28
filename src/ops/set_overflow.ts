/**
 * port.ts -- pure-TS port of MPFR's `mpfr_set_overflow`.
 *
 * Sets the OVERFLOW bit (bit 1 = MPFR_FLAGS_OVERFLOW = 2n) in the flag
 * register. The C body mutates the static `__gmpfr_flags` global; mpfr-ts
 * uses an immutable wire-form matching the rest of the flag family: take
 * the pre-set flag state as `mask: bigint`, return the post-set state.
 *
 * Algorithm (mpfr/src/exceptions.c L208-L214):
 *
 *   __gmpfr_flags |= MPFR_FLAGS_OVERFLOW
 *
 * i.e. set the OVERFLOW bit in the 6-bit register (ALL=63). Bits outside
 * MPFR_FLAGS_ALL are masked off on entry via setFlags, matching the
 * `src/internal/mpfr/flags.ts` contract.
 *
 * Ref: mpfr/src/exceptions.c L208-L214 -- C reference body.
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_OVERFLOW,
} from '../internal/mpfr/flags.ts';

/**
 * Set the OVERFLOW flag bit in `mask` and return the resulting register state.
 *
 * @mpfrName mpfr_set_overflow
 *
 * @param mask  Pre-set flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off inside setFlags.
 * @returns     Post-set register state with the OVERFLOW bit set.
 */
export function mpfr_set_overflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_overflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(mask);
  setFlags(MPFR_FLAGS_OVERFLOW);
  return getFlags();
}
