/**
 * ops/set_divby0.ts -- pure-TS port of MPFR's `mpfr_set_divby0`.
 *
 * Sets the DIVBY0 bit in the flag register. The C body mutates the
 * static `__gmpfr_flags` global; mpfr-ts uses an immutable wire-form
 * matching the rest of the flag family: take the pre-set flag state as
 * `mask: bigint`, return the post-set state.
 *
 * Algorithm (mpfr/src/exceptions.c L216-L222):
 *
 *   __gmpfr_flags |= MPFR_FLAGS_DIVBY0
 *
 * i.e. set bit 5 (DIVBY0=32) in the 6-bit register (ALL=63). Bits
 * outside MPFR_FLAGS_ALL are masked off on entry to match the
 * `src/internal/mpfr/flags.ts` `setFlags` contract.
 *
 * Ref: mpfr/src/exceptions.c L216-L222 -- C reference body.
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_DIVBY0,
} from '../internal/mpfr/flags.ts';

/**
 * Set the DIVBY0 flag bit in `mask` and return the resulting register
 * state.
 *
 * @mpfrName mpfr_set_divby0
 *
 * @param mask  Pre-set flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off by the substrate.
 * @returns     Post-set register state with bit 5 (DIVBY0) set.
 */
export function mpfr_set_divby0(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_divby0: mask must be bigint, got ${typeof mask}`,
    );
  }
  // Route through the shared flag register so the substrate stays the
  // canonical source of truth for bit values.
  clearFlags();
  setFlags(mask);
  setFlags(MPFR_FLAGS_DIVBY0);
  return getFlags();
}
