/**
 * ops/erangeflag_p.ts -- pure-TS port of MPFR's `mpfr_erangeflag_p`.
 *
 * "Was an out-of-range condition raised?" predicate. Reads the global
 * MPFR flag register and returns `true` iff the `MPFR_FLAGS_ERANGE` bit
 * (bit 4, value 16) is set.
 *
 * C signature
 * -----------
 *
 *   int mpfr_erangeflag_p(void);
 *
 *   Reads the static `__gmpfr_flags` register and returns the AND with
 *   `MPFR_FLAGS_ERANGE` as an int (0 or 16).
 *
 *   Ref: mpfr/src/exceptions.c L377-L382 -- C reference body.
 *   Ref: mpfr/src/exceptions.c L37-L40   -- `__gmpfr_flags` declaration.
 *
 * TS signature
 * ------------
 *
 *   mpfr_erangeflag_p(mask: bigint): boolean
 *
 * Divergence
 * ----------
 *
 *   1. Return type: C int -> TS boolean (ADR 0001 idiomatic-TS lift for
 *      the `_p` predicate family).
 *
 *   2. Parameter list: C takes void; TS takes `mask: bigint` describing
 *      the desired pre-test flag state. The golden driver calls
 *      `mpfr_clear_flags()` then `mpfr_flags_set(mask)` before reading
 *      the predicate; the TS port mirrors via clearFlags + setFlags.
 *
 *   3. Global flag state: C reads `__gmpfr_flags`; TS routes through the
 *      module-level register in `src/internal/mpfr/flags.ts`. Per-worker
 *      isolation (CLAUDE.md Rule 4) guarantees a fresh register per case.
 *
 *   Note: `MPFR_FLAGS_ERANGE = 16n` (bit 4). Common off-by-position bugs
 *   test bit 4 (NAN) or bit 32 (DIVBY0) instead.
 *
 * Ref: /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit values.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_ERANGE,
} from '../internal/mpfr/flags.ts';

/**
 * Test whether the ERANGE (out-of-range) flag is currently set.
 *
 * @mpfrName mpfr_erangeflag_p
 *
 * @param mask  Flag bits to install before reading the predicate.
 * @returns     `true` iff `(mask & MPFR_FLAGS_ERANGE) !== 0n`.
 */
export function mpfr_erangeflag_p(mask: bigint): boolean {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_erangeflag_p: mask must be a bigint, got ${typeof mask}`,
    );
  }
  // Ref: mpfr/src/exceptions.c L381 -- `__gmpfr_flags & MPFR_FLAGS_ERANGE`.
  clearFlags();
  setFlags(mask);
  return (getFlags() & MPFR_FLAGS_ERANGE) !== 0n;
}
