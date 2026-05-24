/**
 * ops/overflow_p.ts — pure-TS port of MPFR's `mpfr_overflow_p`.
 *
 * "Did an overflow exception occur?" predicate. Tests whether the
 * `MPFR_FLAGS_OVERFLOW` bit (bit 1, value 2) is set in the global flag
 * register after composing a `clearFlags → setFlags(mask)` preamble.
 *
 * C signature
 * -----------
 *
 *   int mpfr_overflow_p(void);
 *
 *   Reads the global `__gmpfr_flags` register and returns the bitwise AND
 *   with `MPFR_FLAGS_OVERFLOW`. The C body is three lines:
 *
 *     MPFR_COLD_FUNCTION_ATTR int
 *     mpfr_overflow_p(void) {
 *       MPFR_STAT_STATIC_ASSERT(MPFR_FLAGS_OVERFLOW <= INT_MAX);
 *       return __gmpfr_flags & MPFR_FLAGS_OVERFLOW;
 *     }
 *
 *   Ref: mpfr/src/exceptions.c L339–L346 — the C reference body.
 *   Ref: mpfr/src/exceptions.c L37–L40   — `__gmpfr_flags` declaration.
 *
 * TS signature (wire form)
 * ------------------------
 *
 *   mpfr_overflow_p(mask: bigint): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Three divergences (see spec.json `divergence_from_c`):
 *
 *   1. Return type: C returns `int` (0 or non-zero); TS returns `boolean`
 *      per ADR 0001 idiomatic-TS lift for the `_p` predicate family.
 *
 *   2. Parameter: C takes `void`. The wire form adds `mask: bigint` to
 *      encode the desired pre-test flag-register state (UNDERFLOW=1,
 *      OVERFLOW=2, NAN=4, INEXACT=8, ERANGE=16, DIVBY0=32; OR-combine).
 *      The C golden driver performs `mpfr_clear_flags(); mpfr_flags_set(mask)`
 *      before reading the predicate; this port mirrors that composition.
 *
 *   3. Global flag state: C reads the static `__gmpfr_flags`. TS uses the
 *      module `src/internal/mpfr/flags.ts` (`clearFlags`, `setFlags`,
 *      `MPFR_FLAGS_OVERFLOW`) which is reset to 0n on every worker init
 *      (CLAUDE.md Rule 4 per-case isolation).
 *
 * Algorithm
 * ---------
 *
 *   1. `clearFlags()` — wipe the register (mirrors `mpfr_clear_flags()`).
 *   2. `setFlags(mask)` — OR `mask` into the register.
 *   3. Return `(register & MPFR_FLAGS_OVERFLOW) !== 0n` as boolean.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/exceptions.c L339–L346 — C reference body.
 *   - mpfr/src/exceptions.c L37–L40   — `__gmpfr_flags` static declaration.
 *   - /usr/include/mpfr.h L77–L88     — `MPFR_FLAGS_*` bit constants.
 *   - src/internal/mpfr/flags.ts      — TS flag-register module.
 *   - docs/adr/0001-spec-merge-policy.md — `_p` predicates return boolean.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_OVERFLOW,
} from '../internal/mpfr/flags.ts';

/**
 * Test whether the overflow flag is set, given the desired pre-test
 * flag-register state `mask`.
 *
 * @param mask   bigint bitmask to load into the flag register before
 *               testing. Bits outside `MPFR_FLAGS_ALL` (63n) are silently
 *               masked by `setFlags`. See `/usr/include/mpfr.h L77–L88`
 *               for the six defined bit values.
 * @returns      `true` iff `MPFR_FLAGS_OVERFLOW` is set after loading `mask`;
 *               `false` otherwise.
 * @throws {MPFRError} `EDOMAIN` if `mask` is not a bigint at runtime (Law 1:
 *         fail fast on invariant violation at the public API boundary).
 *
 * @mpfrName mpfr_overflow_p
 *
 * @example
 *   mpfr_overflow_p(0n);                    // false — all flags clear
 *   mpfr_overflow_p(2n);                    // true  — OVERFLOW bit set
 *   mpfr_overflow_p(63n);                   // true  — all flags set
 *   mpfr_overflow_p(61n);                   // false — OVERFLOW bit clear (63 ^ 2)
 */
export function mpfr_overflow_p(mask: bigint): boolean {
  // Law 1: fail fast on malformed input at the public boundary.
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mask must be bigint, got ${typeof mask}`);
  }
  // mpfr/src/exceptions.c L339–L346: clear register, load mask, read bit.
  clearFlags();
  setFlags(mask);
  return (getFlags() & MPFR_FLAGS_OVERFLOW) > 0n;
}
