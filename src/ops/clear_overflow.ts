/**
 * ops/clear_overflow.ts -- pure-TS port of MPFR's `mpfr_clear_overflow`.
 *
 * Clears the OVERFLOW bit (bit 1 = 2) from the flag register.
 *
 * Algorithm (mpfr/src/exceptions.c L160-L166):
 *
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW   // mask & 61n
 *
 * Ref: mpfr/src/exceptions.c L160-L166 -- C reference.
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
 * Clear the OVERFLOW flag bit from `mask` and return the resulting state.
 *
 * @mpfrName mpfr_clear_overflow
 *
 * @param mask  Pre-clear flag state. Out-of-domain bits silently masked off.
 * @returns     Post-clear register state with bit 1 (OVERFLOW) cleared.
 */
export function mpfr_clear_overflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_overflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(mask);
  clearFlags(MPFR_FLAGS_OVERFLOW);
  return getFlags();
}
