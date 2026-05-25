/**
 * ops/clear_underflow.ts -- pure-TS port of MPFR's `mpfr_clear_underflow`.
 *
 * Clears the UNDERFLOW bit (bit 0 = 1) from the flag register.
 *
 * Algorithm (mpfr/src/exceptions.c L152-L158):
 *
 *   __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_UNDERFLOW   // mask & 62n
 *
 * Ref: mpfr/src/exceptions.c L152-L158 -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_UNDERFLOW,
} from '../internal/mpfr/flags.ts';

/**
 * Clear the UNDERFLOW flag bit from `mask` and return the resulting state.
 *
 * @mpfrName mpfr_clear_underflow
 *
 * @param mask  Pre-clear flag state. Out-of-domain bits silently masked off.
 * @returns     Post-clear register state with bit 0 (UNDERFLOW) cleared.
 */
export function mpfr_clear_underflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_underflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  clearFlags();
  setFlags(mask);
  clearFlags(MPFR_FLAGS_UNDERFLOW);
  return getFlags();
}
