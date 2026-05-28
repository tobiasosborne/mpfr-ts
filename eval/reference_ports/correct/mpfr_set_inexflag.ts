/**
 * reference_ports/correct/mpfr_set_inexflag.ts -- hand-written reference port of MPFR's
 * `mpfr_set_inexflag`, for golden calibration only (NOT the shipped port).
 *
 * Sets the INEXACT bit in the flag register. The C body mutates the static
 * `__gmpfr_flags` global; mpfr-ts uses an immutable wire-form matching the
 * rest of the flag family: take the pre-set flag state as `mask: bigint`,
 * return the post-set state.
 *
 * Algorithm (mpfr/src/exceptions.c L232-L238):
 *
 *   __gmpfr_flags |= MPFR_FLAGS_INEXACT
 *
 * i.e. set bit 8 in the 6-bit register (ALL=63). Bits outside
 * MPFR_FLAGS_ALL are masked off on entry to match the
 * `src/internal/mpfr/flags.ts` `setFlags` contract.
 *
 * Ref: mpfr/src/exceptions.c L232-L238 -- C reference body.
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants.
 * Ref: src/internal/mpfr/flags.ts -- shipped TS flag register.
 */

import { MPFRError } from '../../../src/core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_INEXACT,
} from '../../../src/internal/mpfr/flags.ts';

/**
 * Set the INEXACT flag bit in `mask` and return the resulting register state.
 *
 * @mpfrName mpfr_set_inexflag
 *
 * @param mask  Pre-set flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *              silently masked off.
 * @returns     Post-set register state with bit 8 (INEXACT) set.
 */
export function mpfr_set_inexflag(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_inexflag: mask must be bigint, got ${typeof mask}`,
    );
  }
  // Route through the shared flag register so the substrate stays the
  // canonical source of truth for bit values.
  clearFlags();
  setFlags(mask);
  setFlags(MPFR_FLAGS_INEXACT);
  return getFlags();
}
