/**
 * ops/flags_save.ts -- pure-TS port of MPFR's `mpfr_flags_save`.
 *
 * Pure read: return the current flag register, snapped to the 6-bit
 * MPFR_FLAGS_ALL domain. The C body is `return __gmpfr_flags`; the TS
 * wire form takes the desired prior state `pre` and returns
 * `pre & MPFR_FLAGS_ALL`.
 *
 * Algorithm (mpfr/src/exceptions.c L125-L131):
 *
 *   return __gmpfr_flags
 *
 * Ref: mpfr/src/exceptions.c L125-L131 -- C reference body.
 * Ref: src/internal/mpfr/flags.ts -- getFlags() is the canonical read.
 */

import { MPFRError } from '../core.ts';
import { clearFlags, setFlags, getFlags } from '../internal/mpfr/flags.ts';

/**
 * Read (a copy of) the flag register seeded with `pre`.
 *
 * @mpfrName mpfr_flags_save
 *
 * @param pre   Desired prior flag state. Bits outside MPFR_FLAGS_ALL are
 *              silently masked off.
 * @returns     `pre & 63n` -- the masked prior state.
 */
export function mpfr_flags_save(pre: bigint): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_save: pre must be bigint, got ${typeof pre}`,
    );
  }
  clearFlags();
  setFlags(pre);
  return getFlags();
}
