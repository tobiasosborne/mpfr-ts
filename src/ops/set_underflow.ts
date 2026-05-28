/**
 * ops/set_underflow.ts -- pure-TS port of MPFR's `mpfr_set_underflow`.
 *
 * Sets the UNDERFLOW bit in the flag register. The C body mutates the static
 * `__gmpfr_flags` global; mpfr-ts uses an immutable wire-form matching the
 * rest of the flag family: take the pre-set flag state as `mask: bigint`,
 * return the post-set state.
 *
 * Algorithm (mpfr/src/exceptions.c L200-L206):
 *
 *   __gmpfr_flags |= MPFR_FLAGS_UNDERFLOW
 *
 * i.e. set bit 1 in the 6-bit register (ALL=63). Bits outside
 * MPFR_FLAGS_ALL are masked off on entry to match the
 * `src/internal/mpfr/flags.ts` `setFlags` contract.
 *
 * Ref: mpfr/src/exceptions.c L200-L206 -- C reference body.
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from "../core.ts";
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_UNDERFLOW,
} from "../internal/mpfr/flags.ts";

/**
 * Set the UNDERFLOW flag bit in `mask` and return the resulting register state.
 *
 * @mpfrName mpfr_set_underflow
 *
 * @param mask  Pre-set flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off.
 * @returns     Post-set register state with bit 1 (UNDERFLOW) set.
 */
export function mpfr_set_underflow(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_underflow: mask must be bigint, got ${typeof mask}`,
    );
  }
  // Route through the shared flag register so the substrate stays the
  // canonical source of truth for bit values.
  clearFlags();
  setFlags(mask);
  setFlags(MPFR_FLAGS_UNDERFLOW);
  return getFlags();
}
