/**
 * ops/divby0_p.ts — pure-TS port of MPFR's `mpfr_divby0_p`.
 *
 * "Was a division-by-zero raised?" predicate. Reads the global MPFR flag
 * register and returns `true` iff the `MPFR_FLAGS_DIVBY0` bit (bit 5,
 * value 32) is set.
 *
 * C signature
 * -----------
 *
 *   int mpfr_divby0_p(void);
 *
 *   Reads the static `__gmpfr_flags` register and returns the AND with
 *   `MPFR_FLAGS_DIVBY0` as an int (0 or 32). The C body is two lines
 *   (excluding the static-assert for bounds safety):
 *
 *     MPFR_COLD_FUNCTION_ATTR int
 *     mpfr_divby0_p (void)
 *     {
 *       MPFR_STAT_STATIC_ASSERT (MPFR_FLAGS_DIVBY0 <= INT_MAX);
 *       return __gmpfr_flags & MPFR_FLAGS_DIVBY0;
 *     }
 *
 *   Ref: mpfr/src/exceptions.c L350–L355 — the C reference.
 *   Ref: mpfr/src/exceptions.c L37–L40   — `__gmpfr_flags` declaration.
 *
 * TS signature
 * ------------
 *
 *   mpfr_divby0_p(mask: bigint): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Three divergences from the C function (documented in spec.json):
 *
 *   1. Return type: C returns `int`; TS returns `boolean` per ADR 0001
 *      idiomatic-TS lift for the `_p` predicate family.
 *
 *   2. Parameter list: C takes `void`. The TS wire form takes `mask:
 *      bigint` — the desired pre-test flag state. The golden driver calls
 *      `mpfr_clear_flags()` then `mpfr_flags_set(mask)` before reading the
 *      predicate; the TS port must perform the same composition.
 *
 *   3. Global flag state: C reads `__gmpfr_flags`, a static in
 *      mpfr/src/exceptions.c. The TS equivalent is the module-level
 *      register in `src/internal/mpfr/flags.ts`. Per-worker isolation
 *      (CLAUDE.md Rule 4) guarantees each test case starts from `0n`.
 *
 * Algorithm
 * ---------
 *
 *   1. `clearFlags()` — reset the register (mirrors `mpfr_clear_flags()`).
 *   2. `setFlags(mask)` — set the caller-requested bits (mirrors `mpfr_flags_set(mask)`).
 *   3. Return `(getFlags() & MPFR_FLAGS_DIVBY0) !== 0n` as a boolean.
 *
 *   `MPFR_FLAGS_DIVBY0 = 32n` (bit 5). Note it is the *highest*-valued of
 *   the six MPFR flags; a common off-by-position bug is to test bit 0x02
 *   (OVERFLOW) or to confuse the bit with its ordinal index.
 *
 *   Ref: /usr/include/mpfr.h L77–L88 — MPFR_FLAGS_* bit values.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_DIVBY0,
} from '../internal/mpfr/flags.ts';

/**
 * Test whether the division-by-zero flag is currently set.
 *
 * The wire form takes `mask: bigint` describing the desired pre-test state
 * of the flag register. The function clears the register, sets `mask`,
 * then reads `MPFR_FLAGS_DIVBY0` (bit 5 = 32n). Bits outside
 * `MPFR_FLAGS_ALL` (63n) are silently ignored by `setFlags` per the
 * flags module contract. A non-bigint `mask` throws `EDOMAIN`
 * (fail-fast per CLAUDE.md Rule 1).
 *
 * @param mask   Flag bits to install before reading the predicate.
 * @returns      `true` iff `(mask & MPFR_FLAGS_DIVBY0) !== 0n`.
 *
 * @mpfrName mpfr_divby0_p
 */
export function mpfr_divby0_p(mask: bigint): boolean {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_divby0_p: mask must be a bigint, got ${typeof mask}`,
    );
  }
  // Ref: mpfr/src/exceptions.c L354 — `__gmpfr_flags & MPFR_FLAGS_DIVBY0`.
  // Compose: clear → set mask → read DIVBY0 bit.
  clearFlags();
  setFlags(mask);
  return (getFlags() & MPFR_FLAGS_DIVBY0) > 0n;
}
