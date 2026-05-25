/**
 * ops/clear_divby0.ts -- pure-TS port of MPFR's `mpfr_clear_divby0`.
 *
 * Clears the DIVBY0 bit from the flag register. The C body mutates the
 * static `__gmpfr_flags` global; mpfr-ts uses an immutable wire-form
 * matching the rest of the flag family: take the pre-clear flag state as
 * `mask: bigint`, return the post-clear state.
 *
 * Algorithm (mpfr/src/exceptions.c L168-L174):
 *
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0
 *
 * i.e. clear bit 5 (DIVBY0=32) from the 6-bit register (ALL=63). Bits
 * outside MPFR_FLAGS_ALL are masked off on entry to match the
 * `src/internal/mpfr/flags.ts` `setFlags` contract.
 *
 * Ref: mpfr/src/exceptions.c L168-L174 -- C reference body.
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
 * Clear the DIVBY0 flag bit from `mask` and return the resulting register
 * state.
 *
 * @mpfrName mpfr_clear_divby0
 *
 * @param mask  Pre-clear flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off.
 * @returns     Post-clear register state with bit 5 (DIVBY0) cleared.
 */
export function mpfr_clear_divby0(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_divby0: mask must be bigint, got ${typeof mask}`,
    );
  }
  // Route through the shared flag register so the substrate stays the
  // canonical source of truth for bit values.
  clearFlags();
  setFlags(mask);
  clearFlags(MPFR_FLAGS_DIVBY0);
  return getFlags();
}
