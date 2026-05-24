/**
 * ops/equal_p.ts — pure-TS port of MPFR's `mpfr_equal_p`.
 *
 * Equality predicate. Returns `true` iff `a == b` under the MPFR
 * ordering, `false` otherwise — including when either operand is NaN.
 *
 * **Signed zero is NOT distinguished by this predicate**: `+0 == -0`
 * returns `true`, because the underlying {@link compareMPFR} (via
 * mpfr_cmp) treats `+0` and `-0` as equal for ordering. To distinguish
 * signed zeros one must inspect `kind === 'zero' && sign` directly,
 * or use the sign-of-zero op once ported.
 *
 * **NaN comparison returns false**: `equal_p(NaN, NaN)` is `false`,
 * matching IEEE 754 and the C MPFR contract. This is one of the few
 * places where the TS surface DOES propagate the "NaN ≠ NaN" rule —
 * because the predicate's return-type is `boolean` with a documented
 * non-throwing NaN semantic. (Contrast with `mpfr_cmp`, which throws.)
 *
 * C signature
 * -----------
 *
 *   int mpfr_equal_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   See mpfr/src/comparisons.c L69–L73:
 *
 *     int mpfr_equal_p (mpfr_srcptr x, mpfr_srcptr y) {
 *       return MPFR_IS_NAN(x) || MPFR_IS_NAN(y) ? 0 : (mpfr_cmp (x, y) == 0);
 *     }
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The predicate family does NOT throw on NaN — returns `false`. Return
 * type is TS `boolean`. See `src/ops/less_p.ts` for the rationale.
 *
 * Algorithm
 * ---------
 *
 * Delegates to {@link compareMPFR}:
 *
 *   1. NaN → null → predicate returns `false`.
 *   2. Else → result === 0 ⇔ a == b.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/comparisons.c L69–L73.
 *   - src/internal/mpfr/cmp_raw.ts.
 *   - mpfr/tests/tcomparisons.c — source for the `mined` cases. The
 *     `eq_tests` routine (L87–L117) specifically asserts that same-value
 *     comparisons at different precisions return equal.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — equal_p
 *     returning false for NaN/NaN is a deliberate IEEE 754 alignment.
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *     for ARITHMETIC ops; for cmp/equal_p the C reference treats +0/-0
 *     as equal.
 */

import type { MPFR } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Test `a == b`. Returns `false` if either operand is NaN.
 *
 * @param a left operand.
 * @param b right operand.
 * @returns `true` iff `a` and `b` represent the same MPFR value; `false`
 *   otherwise. Signed zero is NOT distinguished (`+0` equals `-0`).
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation. Does NOT throw on NaN.
 *
 * @mpfrName mpfr_equal_p
 */
export function mpfr_equal_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  return c === null ? false : c === 0;
}
