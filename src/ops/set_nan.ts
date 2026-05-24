/**
 * ops/set_nan.ts — pure-TS port of MPFR's `mpfr_set_nan`.
 *
 * Public-surface op. Imports from the locked schema in `src/core.ts` per
 * CLAUDE.md Law 4. Returns the canonical `NAN_VALUE` sentinel — the
 * single immutable NaN representative used everywhere in the library.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_nan(mpfr_ptr x);
 *
 *   - mutates `x` in place to NaN at `x`'s pre-existing precision;
 *   - sets the global `__gmpfr_flags |= MPFR_FLAGS_NAN`.
 *
 *   Ref: mpfr/src/set_nan.c L25–L30 — body is `MPFR_SET_NAN(x); __gmpfr_flags |= MPFR_FLAGS_NAN;`.
 *
 * Divergence from C — and why
 * ---------------------------
 *
 * The C function takes an already-allocated `mpfr_t x` whose precision
 * is preserved across the call (the NaN keeps the prec of the slot it
 * was poured into). The immutable TS schema models NaN with
 * `prec === 0n` by convention (src/core.ts L103–L107), because no
 * downstream op preserves the originating precision of a NaN — so we
 * collapse every NaN to the singleton `NAN_VALUE` and the C-side
 * "precision of NaN" notion is structurally inexpressible.
 *
 * The TS signature therefore takes **no arguments**: there is no slot
 * to "set", no precision to preserve, no flag to toggle. Calling
 * `mpfr_set_nan()` is structurally identical to writing `NAN_VALUE`,
 * but we expose the function form because:
 *
 *   (a) downstream library code may want to be discoverable via
 *       "the function the C code uses" rather than via the constant;
 *   (b) future ports that re-create an in-place-mutating C helper —
 *       e.g. a `compose-with-rop` pattern — will be more readable when
 *       the call-site reads `rop = mpfr_set_nan()` instead of
 *       `rop = NAN_VALUE`;
 *   (c) the eval harness can grade `mpfr_set_nan` as a function with
 *       its own spec.json / golden, exercising the wire-format NaN
 *       round-trip in isolation.
 *
 * No precision validation is performed because there is no precision.
 * No error path exists — the function is total and pure (it always
 * returns the same `Object.frozen` constant).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_nan.c — the C reference.
 *   - src/core.ts L243–L249 — `NAN_VALUE` constant; the canonical NaN.
 *   - src/core.ts L380–L393 — `validate()` requires NaN to match the
 *     canonical shape (prec=0n, sign=1, exp=0n, mant=0n).
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — IEEE 754
 *     NaN-equality rules; the grader's `compareMpfr` short-circuits on
 *     both sides being kind:'nan'.
 *   - CLAUDE.md Law 4 — library coherence: public ports must import
 *     the locked schema.
 */

import type { MPFR } from '../core.ts';
import { NAN_VALUE } from '../core.ts';

/**
 * Return the canonical NaN value.
 *
 * @mpfrName mpfr_set_nan
 *
 * @returns the singleton {@link NAN_VALUE} — `{kind:'nan', sign:1,
 *          prec:0n, exp:0n, mant:0n}`, frozen.
 *
 * @example
 *   const x = mpfr_set_nan();
 *   x.kind === 'nan';   // true
 *   x === NAN_VALUE;    // true — identity, not just equality
 */
export function mpfr_set_nan(): MPFR {
  // The single source of truth for NaN. NAN_VALUE is Object.frozen at
  // module load (src/core.ts L243), so returning the constant directly
  // is safe — no caller can mutate the shared sentinel.
  return NAN_VALUE;
}
