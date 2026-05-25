/**
 * ops/clear_inexflag.ts -- pure-TS port of MPFR's `mpfr_clear_inexflag`.
 *
 * Clears the INEXACT bit (bit 3 = 8) from the flag register.
 *
 * Algorithm (mpfr/src/exceptions.c L184-L190):
 *
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_INEXACT   // mask & 55n
 *
 * Ref: mpfr/src/exceptions.c L184-L190 -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_INEXACT,
} from '../internal/mpfr/flags.ts';

/**
 * Clear the INEXACT flag bit from `mask` and return the resulting state.
 *
 * @mpfrName mpfr_clear_inexflag
 *
 * @param mask  Pre-clear flag state. Out-of-domain bits silently masked off.
 * @returns     Post-clear register state with bit 3 (INEXACT) cleared.
 */
export function mpfr_clear_inexflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_inexflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(mask);
  clearFlags(MPFR_FLAGS_INEXACT);
  return getFlags();
}
