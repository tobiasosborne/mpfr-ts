/**
 * ops/greaterequal_p.ts — pure-TS port of MPFR's `mpfr_greaterequal_p`.
 *
 * "Greater-than-or-equal" predicate. Returns `true` iff `a >= b` under
 * the MPFR ordering, `false` otherwise — including when either operand
 * is NaN.
 *
 * C signature
 * -----------
 *
 *   int mpfr_greaterequal_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   See mpfr/src/comparisons.c L45–L49:
 *
 *     int mpfr_greaterequal_p (mpfr_srcptr x, mpfr_srcptr y) {
 *       return MPFR_IS_NAN(x) || MPFR_IS_NAN(y) ? 0 : (mpfr_cmp (x, y) >= 0);
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
 *   2. Else → result >= 0 ⇔ a > b OR a == b.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/comparisons.c L45–L49.
 *   - src/internal/mpfr/cmp_raw.ts.
 *   - mpfr/tests/tcomparisons.c — source for the `mined` cases.
 */

import type { MPFR } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Test `a >= b`. Returns `false` if either operand is NaN.
 *
 * @param a left operand.
 * @param b right operand.
 * @returns `true` iff `a > b` or `a == b`; `false` otherwise.
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation. Does NOT throw on NaN.
 *
 * @mpfrName mpfr_greaterequal_p
 */
export function mpfr_greaterequal_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  return c === null ? false : c >= 0;
}
