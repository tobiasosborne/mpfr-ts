/**
 * ops/lessgreater_p.ts — pure-TS port of MPFR's `mpfr_lessgreater_p`.
 *
 * "Less or greater" predicate — strict-inequality. Returns `true` iff
 * `a < b` OR `a > b`. Returns `false` when `a == b` (signed zero
 * collapses to equal — see below) and on NaN (the "unordered" case).
 *
 * Equivalent to `!equal_p(a, b) && !unordered_p(a, b)`, i.e. "a and b
 * are ordered and unequal".
 *
 * C signature
 * -----------
 *
 *   int mpfr_lessgreater_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   See mpfr/src/comparisons.c L63–L67:
 *
 *     int mpfr_lessgreater_p (mpfr_srcptr x, mpfr_srcptr y) {
 *       return MPFR_IS_NAN(x) || MPFR_IS_NAN(y) ? 0 : (mpfr_cmp (x, y) != 0);
 *     }
 *
 *   The truth-table row from mpfr/src/comparisons.c L27–L36:
 *
 *                            =     <     >     unordered
 *     mpfr_lessgreater_p     0     1     1     0
 *
 *   (False for equal AND for unordered; true only when strictly ordered
 *   and unequal.)
 *
 * TS signature
 * ------------
 *
 *   mpfr_lessgreater_p(a: MPFR, b: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Same as the rest of the predicate family: no throw on NaN, return
 * type is TS `boolean` rather than `int`. See `src/ops/less_p.ts`
 * §"Divergence from C → TS" for the shared rationale.
 *
 * Algorithm
 * ---------
 *
 * Delegates to {@link compareMPFR}:
 *
 *   1. NaN → null → predicate returns `false`. (C: NaN-guard returns
 *      0; row 4 of the truth table.)
 *   2. Else → -1 / 0 / +1; predicate returns `result !== 0`.
 *      (`result === 0` ⇔ a == b; both sides false. `result === ±1` ⇔
 *      a < b or a > b; both sides true.)
 *
 * Signed zero
 * -----------
 *
 * `compareMPFR(+0, -0)` and `compareMPFR(-0, +0)` both return `0`
 * (signed zero is NOT ordered for cmp — see src/internal/mpfr/cmp_raw.ts
 * §"Step 2"). So `lessgreater_p(+0, -0)` returns `false`, matching
 * the C reference. The signed-zero-is-real callout (CLAUDE.md
 * Hallucination-risk) applies to ARITHMETIC ops, not to the cmp
 * family.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/comparisons.c L63–L67 — the C reference.
 *   - mpfr/src/comparisons.c L24–L37 — the truth table.
 *   - src/internal/mpfr/cmp_raw.ts — the shared comparison core
 *     used by every predicate.
 *   - src/ops/equal_p.ts — the dual predicate; `!equal_p(a,b) &&
 *     !unordered_p(a,b)` is the lessgreater_p decomposition (though
 *     we don't implement it that way to avoid two compareMPFR calls).
 *   - src/ops/less_p.ts — sibling for the shared non-throwing NaN
 *     contract.
 *   - mpfr/tests/tcomparisons.c L26–L84 — source for the `mined` cases.
 *     The cmp_tests routine asserts `lessgreater_p` is true iff cmp
 *     is nonzero (and both operands are non-NaN).
 *   - CLAUDE.md "Hallucination-risk callouts": NaN ≠ NaN (predicates
 *     return false for unordered); Signed zero is real (NOT for cmp,
 *     so +0 vs -0 → lessgreater_p false).
 */

import type { MPFR } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Test `a < b || a > b`. Returns `false` when `a == b` (including
 * signed zero collapse, since cmp does not order signed zero) or when
 * either operand is NaN.
 *
 * @param a left operand.
 * @param b right operand.
 * @returns `true` iff `a` and `b` are ordered AND not equal; `false`
 *   otherwise.
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation (delegated to compareMPFR). Does NOT throw on NaN.
 *
 * @mpfrName mpfr_lessgreater_p
 */
export function mpfr_lessgreater_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  // NaN → false (unordered). Otherwise: nonzero ↔ strictly ordered
  // (less or greater). Equal → 0 → false.
  return c === null ? false : c !== 0;
}
