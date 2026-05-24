/**
 * ops/less_p.ts — pure-TS port of MPFR's `mpfr_less_p`.
 *
 * "Strictly less than" predicate. Returns `true` iff `a < b` under the
 * MPFR ordering, `false` otherwise — including when either operand is
 * NaN (the "unordered" case in IEEE 754 terms).
 *
 * C signature
 * -----------
 *
 *   int mpfr_less_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   Returns non-zero iff `x < y`, zero otherwise. NaN-vs-anything is
 *   zero. See mpfr/src/comparisons.c L52–L55:
 *
 *     int mpfr_less_p (mpfr_srcptr x, mpfr_srcptr y) {
 *       return MPFR_IS_NAN(x) || MPFR_IS_NAN(y) ? 0 : (mpfr_cmp (x, y) < 0);
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_less_p(a: MPFR, b: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Unlike {@link import('./cmp.ts').mpfr_cmp}, which throws `MPFRError`
 * on NaN, the predicate family DOES NOT throw on NaN — it returns
 * `false` (matching the C contract of "unordered → 0"). The rationale
 * is that C MPFR's predicates have a well-defined non-throwing semantics
 * for NaN (returning 0=false), so the TS port preserves that contract
 * rather than inventing a divergent throw. Callers chaining predicates
 * (`!less_p(a, b) && !greater_p(a, b)` to test "equal-or-unordered")
 * benefit from this consistency.
 *
 * The return type is TS `boolean` rather than `number`/`int` — the TS
 * surface uses native booleans for what C encodes as `int`. The
 * grader's `jl_output_scalar_bool` wire helper emits bare-JSON booleans
 * to match.
 *
 * Algorithm
 * ---------
 *
 * Delegates to the shared {@link compareMPFR} core in
 * `src/internal/mpfr/cmp_raw.ts`, mirroring the C composition pattern:
 *
 *   1. If either operand is NaN, compareMPFR returns null — predicate
 *      returns `false`. (C: NaN-guard returns 0.)
 *   2. Otherwise compareMPFR returns -1 / 0 / +1; less_p returns
 *      `result < 0`. (C: `mpfr_cmp(x, y) < 0`.)
 *
 * Refs
 * ----
 *
 *   - mpfr/src/comparisons.c L52–L55 — the C reference.
 *   - mpfr/src/cmp.c L32–L98 — the underlying mpfr_cmp algorithm.
 *   - src/internal/mpfr/cmp_raw.ts — the shared non-throwing core.
 *   - src/ops/cmp.ts — the throwing surface; predicates use the
 *     shared core directly to avoid wrapping a throw in a try/catch.
 *   - mpfr/tests/tcomparisons.c — source for the `mined` cases.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — predicates
 *     return false for NaN; only `mpfr_cmp` diverges to throw.
 */

import type { MPFR } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Test `a < b`. Returns `false` if either operand is NaN.
 *
 * @param a left operand. Must pass {@link import('../core.ts').validate}.
 * @param b right operand. Must pass {@link import('../core.ts').validate}.
 * @returns `true` iff `a < b` strictly; `false` otherwise (including
 *   `a == b`, `a > b`, and the NaN-unordered case).
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation. Does NOT throw on NaN (see "Divergence from C → TS").
 *
 * @mpfrName mpfr_less_p
 */
export function mpfr_less_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  // NaN → false (unordered). Otherwise: strictly less → true.
  return c === null ? false : c < 0;
}
