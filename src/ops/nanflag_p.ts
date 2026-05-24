/**
 * ops/nanflag_p.ts — pure-TS port of MPFR's `mpfr_nanflag_p`.
 *
 * "Is the global NaN flag set?" predicate. Reads the module-level flag
 * register and returns `true` iff the `MPFR_FLAGS_NAN` bit (0x4) is set.
 *
 * IMPORTANT: this is NOT `mpfr_nan_p` (`src/ops/nan_p.ts`), which asks
 * "is this MPFR *value* NaN?". `mpfr_nanflag_p` asks "has the NaN *flag*
 * been raised in the global exception register?" They share three letters
 * and nothing else.
 *
 * C signature
 * -----------
 *
 *   int mpfr_nanflag_p(void);
 *
 *   One-liner: returns `__gmpfr_flags & MPFR_FLAGS_NAN`. If the bit is set
 *   the return value is non-zero (specifically 4); zero otherwise. The
 *   `MPFR_STAT_STATIC_ASSERT` is a compile-time sanity check that the bit
 *   fits in `int`; irrelevant in TS.
 *
 *   Ref: mpfr/src/exceptions.c L357–L364 — full C body.
 *   Ref: mpfr/src/exceptions.c L37–L40  — `__gmpfr_flags` declaration.
 *
 * TS signature
 * ------------
 *
 *   mpfr_nanflag_p(mask: bigint): boolean;
 *
 *   The `mask` parameter is a grader-compatibility shim: because each test
 *   case runs in a fresh Bun worker (CLAUDE.md Rule 4) with a zeroed flag
 *   register, the wire format encodes the desired pre-test flag state as a
 *   `bigint`. The port must compose:
 *     1. `clearFlags()` — wipe the register.
 *     2. `setFlags(mask)` — install the desired state.
 *     3. read `MPFR_FLAGS_NAN` bit from the register.
 *   This mirrors exactly what the C golden driver does via
 *   `mpfr_clear_flags() + mpfr_flags_set(mask) + mpfr_nanflag_p()`.
 *
 * Divergence from C → TS
 * ----------------------
 *
 *   1. Return type: C returns `int` (0 or 4); TS returns `boolean` per
 *      ADR 0001 idiomatic-TS lift for the `_p` predicate family.
 *   2. Parameter list: C takes `void`; TS takes `mask: bigint` for the
 *      grader-stateless wire protocol (see spec.json divergence_from_c).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/exceptions.c L357–L364 — C reference body.
 *   - src/internal/mpfr/flags.ts — the TS flag-state module.
 *   - docs/adr/0001-spec-merge-policy.md — boolean-lift policy.
 *   - eval/functions/mpfr_nanflag_p/spec.json — full spec + divergences.
 */

// Law 4 (Library Coherence) — every public port imports from the locked schema.
// This predicate operates on the flag register, not MPFR values; we import
// MPFRError to report a malformed (non-bigint) mask argument.
import { MPFRError } from '../core.ts';

import {
  clearFlags,
  setFlags,
  getFlags,
} from '../internal/mpfr/flags.ts';

/**
 * Test whether the global NaN exception flag is currently set.
 *
 * The `mask` parameter drives the pre-test flag state (required because
 * each test case runs in a fresh worker with a zeroed register). The port
 * calls `clearFlags() → setFlags(mask)` before reading the NAN bit —
 * matching the C golden driver's `mpfr_clear_flags() + mpfr_flags_set(mask)`.
 *
 * @param mask   desired flag-register state, in [0n, 63n] (MPFR_FLAGS_ALL).
 *               Bits outside `MPFR_FLAGS_ALL` are silently masked off by
 *               `setFlags` (see `src/internal/mpfr/flags.ts`).
 * @returns      `true` iff `MPFR_FLAGS_NAN` (bit 0x4) is set in the register
 *               after installing `mask`; `false` otherwise.
 *
 * @mpfrName mpfr_nanflag_p
 */
export function mpfr_nanflag_p(mask: bigint): boolean {
  // Guard: mask must be a bigint (CLAUDE.md Rule 1 — fail fast on type violations).
  // Out-of-range bits are silently masked off by setFlags per flags.ts contract.
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_nanflag_p: mask must be bigint, got ${typeof mask}`);
  }
  // Ref: mpfr/src/exceptions.c L357–L364 — mirrors the C driver's
  // mpfr_clear_flags() + mpfr_flags_set(mask) + `__gmpfr_flags & MPFR_FLAGS_NAN`.
  // MPFR_FLAGS_NAN = 4n (bit 2); /usr/include/mpfr.h L81.
  clearFlags();
  setFlags(mask);
  return (getFlags() & 4n) !== 0n;
}
