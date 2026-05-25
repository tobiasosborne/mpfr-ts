/**
 * ops/clear_flags.ts -- pure-TS port of MPFR's `mpfr_clear_flags`.
 *
 * Wipes the entire flag register. The C body is `__gmpfr_flags = 0`;
 * mpfr-ts uses the immutable wire form: take the pre-clear state as
 * `mask` (for parity with the sister `mpfr_clear_<flag>` ports) and
 * return `0n` -- always.
 *
 * Algorithm (mpfr/src/exceptions.c L144-L150):
 *
 *   __gmpfr_flags = 0
 *
 * Ref: mpfr/src/exceptions.c L144-L150 -- C reference body.
 * Ref: src/internal/mpfr/flags.ts -- `clearFlags()` with no arg wipes ALL.
 */

import { MPFRError } from '../core.ts';
import { clearFlags, getFlags } from '../internal/mpfr/flags.ts';

/**
 * Clear every bit of the flag register. Output is unconditionally `0n`.
 *
 * @mpfrName mpfr_clear_flags
 *
 * @param mask  Pre-clear flag state. Preserved in the signature for parity
 *              with the sister `mpfr_clear_<flag>` ports; does not affect
 *              the return value.
 * @returns     Always `0n`.
 */
export function mpfr_clear_flags(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_flags: mask must be bigint, got ${typeof mask}`,
    );
  }
  // C: `__gmpfr_flags = 0`. `mask` is consumed for wire-form parity only.
  clearFlags();
  return getFlags();
}
