/**
 * ops/clear_nanflag.ts -- pure-TS port of MPFR's `mpfr_clear_nanflag`.
 *
 * Clears the NAN bit (bit 2 = 4) from the flag register.
 *
 * Algorithm (mpfr/src/exceptions.c L176-L182):
 *
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_NAN   // mask & 59n
 *
 * Ref: mpfr/src/exceptions.c L176-L182 -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_NAN,
} from '../internal/mpfr/flags.ts';

/**
 * Clear the NAN flag bit from `mask` and return the resulting state.
 *
 * @mpfrName mpfr_clear_nanflag
 *
 * @param mask  Pre-clear flag state. Out-of-domain bits silently masked off.
 * @returns     Post-clear register state with bit 2 (NAN) cleared.
 */
export function mpfr_clear_nanflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_nanflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(mask);
  clearFlags(MPFR_FLAGS_NAN);
  return getFlags();
}
