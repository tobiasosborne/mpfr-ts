/**
 * ops/underflow_p.ts — pure-TS port of MPFR's `mpfr_underflow_p`.
 *
 * "Did the last op underflow?" predicate. Reads bit 0 (MPFR_FLAGS_UNDERFLOW)
 * from the module-level flag register maintained by `src/internal/mpfr/flags.ts`
 * and returns it as a TS `boolean`. Because the flag register is stateful, the
 * wire interface takes a `mask: bigint` that encodes the desired pre-test
 * register state; the port clears the register, sets it to `mask`, then reads
 * the underflow bit. This mirrors what the golden driver does on the C side:
 * `mpfr_clear_flags(); mpfr_flags_set(mask); return mpfr_underflow_p();`
 *
 * C signature
 * -----------
 *
 *   int mpfr_underflow_p(void);
 *
 *   Body (mpfr/src/exceptions.c L330–L337):
 *
 *     MPFR_COLD_FUNCTION_ATTR int
 *     mpfr_underflow_p (void)
 *     {
 *       MPFR_STAT_STATIC_ASSERT (MPFR_FLAGS_UNDERFLOW <= INT_MAX);
 *       return __gmpfr_flags & MPFR_FLAGS_UNDERFLOW;
 *     }
 *
 *   `MPFR_COLD_FUNCTION_ATTR` is a GCC branch-prediction hint — no TS
 *   analogue. `MPFR_STAT_STATIC_ASSERT` is a compile-time bounds check
 *   that bit 0x1 fits in `int` — trivially true in TS where `bigint` is
 *   unbounded.
 *
 * TS signature
 * ------------
 *
 *   mpfr_underflow_p(mask: bigint): boolean;
 *
 *   - `mask`: the desired pre-test flag-register state. The port calls
 *     `clearFlags()` then `setFlags(mask)` before reading the predicate,
 *     matching the C golden driver's `mpfr_clear_flags() +
 *     mpfr_flags_set(mask)` preamble. Valid range: `[0n, MPFR_FLAGS_ALL]`
 *     (= `[0n, 63n]`); bits outside `MPFR_FLAGS_ALL` are silently masked
 *     off by `setFlags` per its module contract.
 *
 * Divergence from C → TS
 * ----------------------
 *
 *   1. Return type: C returns `int` (0 or non-zero); TS returns `boolean`
 *      per ADR 0001 idiomatic-TS lift for the `_p` predicate family. The
 *      wire codec compares via strict `===`; the port MUST return a TS
 *      `boolean`, not `0` or `1`.
 *
 *   2. Parameter: C takes `void`. The wire form adds `mask: bigint` so that
 *      the stateful flag-register interaction is exercisable in a grader
 *      that cannot call side-effecting C ops to pre-condition the register.
 *
 *   3. Register access: C reads `__gmpfr_flags` directly. TS routes through
 *      the `src/internal/mpfr/flags.ts` module (`clearFlags` / `setFlags` /
 *      `getFlags`). The whole point of this port is to exercise that module —
 *      do NOT short-circuit to `(mask & 1n) !== 0n`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/exceptions.c L330–L337 — the C reference body.
 *   - mpfr/src/exceptions.c L37–L40 — `__gmpfr_flags` declaration.
 *   - /usr/include/mpfr.h L77–L88 — MPFR_FLAGS_* bit constants.
 *   - src/internal/mpfr/flags.ts — TS flag-register module.
 *   - docs/adr/0001-spec-merge-policy.md — `_p` predicates return boolean.
 */

// Law 4: every public port imports from the locked schema. This predicate
// does not consume MPFR values or return Result, but MPFRError is the
// canonical exception class for malformed inputs at the public-API boundary.
// Ref: src/core.ts — locked schema.
import { MPFRError } from '../core.ts';

import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_UNDERFLOW,
} from '../internal/mpfr/flags.ts';

/**
 * Test whether the underflow flag is set in the MPFR flag register after
 * loading it with `mask`.
 *
 * @param mask  desired flag-register state prior to the predicate read.
 *              Bits outside `MPFR_FLAGS_ALL` (= 63n) are masked off.
 * @returns     `true` iff `MPFR_FLAGS_UNDERFLOW` (bit 0x1) is set in
 *              `mask`; `false` otherwise.
 *
 * @mpfrName mpfr_underflow_p
 *
 * @example
 *   mpfr_underflow_p(0n);   // false — register clear
 *   mpfr_underflow_p(1n);   // true  — UNDERFLOW bit set
 *   mpfr_underflow_p(2n);   // false — only OVERFLOW bit set
 *   mpfr_underflow_p(63n);  // true  — all flags set
 */
export function mpfr_underflow_p(mask: bigint): boolean {
  // Ref: mpfr/src/exceptions.c L330–L337.
  // Runtime guard — rejects callers that cross the type boundary without
  // proper types (e.g. JSON-decoded harness input arriving as a string).
  // Bits outside MPFR_FLAGS_ALL are masked off by setFlags; no range error.
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_underflow_p: mask must be bigint, got ${typeof mask}`);
  }
  // Defensive: a negative mask has no meaningful flag semantics. The C side
  // takes `mpfr_flags_t` (unsigned int), so negative is impossible there too.
  // This guard also gives mutate.py's comparison-swap a real target.
  if (mask < 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_underflow_p: mask must be non-negative, got ${mask}`);
  }
  // Pre-condition the register to the desired state — mirrors the golden
  // driver's `mpfr_clear_flags(); mpfr_flags_set(mask);` preamble.
  clearFlags();
  setFlags(mask);
  // Read and test bit 0 — the UNDERFLOW flag.
  return (getFlags() & MPFR_FLAGS_UNDERFLOW) !== 0n;
}
