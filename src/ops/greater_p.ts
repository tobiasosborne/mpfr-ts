/**
 * ops/greater_p.ts — pure-TS port of MPFR's `mpfr_greater_p`.
 *
 * "Strictly greater than" predicate. Returns `true` iff `a > b` under
 * the MPFR ordering, `false` otherwise — including when either operand
 * is NaN (the "unordered" case in IEEE 754 terms).
 *
 * C signature
 * -----------
 *
 *   int mpfr_greater_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   See mpfr/src/comparisons.c L39–L43:
 *
 *     int mpfr_greater_p (mpfr_srcptr x, mpfr_srcptr y) {
 *       return MPFR_IS_NAN(x) || MPFR_IS_NAN(y) ? 0 : (mpfr_cmp (x, y) > 0);
 *     }
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The predicate family does NOT throw on NaN (unlike {@link
 * import('./cmp.ts').mpfr_cmp}) — it returns `false`, matching the C
 * "unordered → 0" contract. See `src/ops/less_p.ts` for the rationale.
 *
 * Return type is TS `boolean`, not `number`.
 *
 * Algorithm
 * ---------
 *
 * Delegates to {@link compareMPFR}:
 *
 *   1. NaN → null → predicate returns `false`.
 *   2. Else → result > 0 ⇔ a > b strictly.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/comparisons.c L39–L43.
 *   - src/internal/mpfr/cmp_raw.ts.
 *   - mpfr/tests/tcomparisons.c — source for the `mined` cases.
 */

import type { MPFR } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Test `a > b`. Returns `false` if either operand is NaN.
 *
 * @param a left operand.
 * @param b right operand.
 * @returns `true` iff `a > b` strictly; `false` otherwise.
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation. Does NOT throw on NaN.
 *
 * @mpfrName mpfr_greater_p
 */
export function mpfr_greater_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  return c === null ? false : c > 0;
}
