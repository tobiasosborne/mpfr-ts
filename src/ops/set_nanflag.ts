/**
 * ops/set_nanflag.ts -- pure-TS port of MPFR's `mpfr_set_nanflag`.
 *
 * Sets the NAN bit (bit 2) in the flag register. The C body ORs
 * `MPFR_FLAGS_NAN` into the static `__gmpfr_flags` global; this port
 * follows the same immutable wire-form as the rest of the flag family:
 * take the pre-set flag state as `mask: bigint`, return the post-set
 * state.
 *
 * Algorithm (mpfr/src/exceptions.c L224-L230):
 *
 *   __gmpfr_flags |= MPFR_FLAGS_NAN
 *
 * i.e. set bit 2 (NAN=4) into the 6-bit register (ALL=63). The
 * `setFlags` function from the shared flag module is the canonical set
 * path, mirroring the C `mpfr_flags_set` call.
 *
 * Ref: mpfr/src/exceptions.c L224-L230 -- C reference body.
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 * Ref: src/ops/clear_divby0.ts -- sister clear op using the same pattern.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_NAN,
} from '../internal/mpfr/flags.ts';

/**
 * Set the NAN flag bit into `mask` and return the resulting register
 * state.
 *
 * @mpfrName mpfr_set_nanflag
 *
 * @param mask  Pre-set flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off.
 * @returns     Post-set register state with bit 2 (NAN) set.
 */
export function mpfr_set_nanflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_nanflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  // Route through the shared flag register so the substrate stays the
  // canonical source of truth for bit values. This mirrors the pattern
  // used by mpfr_clear_divby0.
  clearFlags();
  setFlags(mask);
  setFlags(MPFR_FLAGS_NAN);
  return getFlags();
}
