/**
 * ops/clear_erangeflag.ts -- pure-TS port of MPFR's `mpfr_clear_erangeflag`.
 *
 * Clears the ERANGE bit (bit 4 = 16) from the flag register. Immutable
 * wire form: take pre-clear state as `mask`, return post-clear state.
 *
 * Algorithm (mpfr/src/exceptions.c L192-L198):
 *
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_ERANGE   // mask & 47n
 *
 * Ref: mpfr/src/exceptions.c L192-L198 -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_ERANGE,
} from '../internal/mpfr/flags.ts';

/**
 * Clear the ERANGE flag bit from `mask` and return the resulting state.
 *
 * @mpfrName mpfr_clear_erangeflag
 *
 * @param mask  Pre-clear flag state. Bits outside MPFR_FLAGS_ALL are
 *              silently masked off.
 * @returns     Post-clear register state with bit 4 (ERANGE) cleared.
 */
export function mpfr_clear_erangeflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_erangeflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(mask);
  clearFlags(MPFR_FLAGS_ERANGE);
  return getFlags();
}
