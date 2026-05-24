/**
 * ops/nan_p.ts — pure-TS port of MPFR's `mpfr_nan_p`.
 *
 * "Is NaN?" predicate. Returns `true` iff `x` is the NaN value, `false`
 * for any other kind (normal, zero, inf).
 *
 * C signature
 * -----------
 *
 *   int mpfr_nan_p(mpfr_srcptr x);
 *
 *   Returns non-zero iff `x` is NaN, zero otherwise. The C body is a
 *   one-liner that defers to the `MPFR_IS_NAN(x)` macro, which inspects
 *   the sentinel exponent `__MPFR_EXP_NAN`. See mpfr/src/isnan.c L24–L28:
 *
 *     int (mpfr_nan_p) (mpfr_srcptr x) {
 *       return MPFR_IS_NAN(x);
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_nan_p(x: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * C returns `int`; TS returns `boolean`. The predicate family does
 * **not** throw on any input — every well-formed {@link MPFR} value has
 * an unambiguous "is NaN?" answer (including NaN itself, which returns
 * `true`). This is the IEEE-754-aligned contract MPFR's manual specifies
 * for the inspection-only predicates (`mpfr_nan_p`, `mpfr_inf_p`,
 * `mpfr_zero_p`, `mpfr_number_p`, `mpfr_signbit`), and is preserved
 * verbatim here.
 *
 * Algorithm
 * ---------
 *
 * The locked schema (`src/core.ts`) makes "is NaN?" a single field
 * comparison: `x.kind === 'nan'`. The C side dispatches on a sentinel
 * exponent value; ours reads the explicit discriminant directly.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/isnan.c L24–L28 — the C reference.
 *   - src/core.ts L73 — `MPFRKind` (the `'nan'` discriminant).
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — for
 *     `mpfr_nan_p` the rule reduces to "ask the kind discriminant" since
 *     our NaN is canonical (no internal-state variants).
 */

import type { MPFR } from '../core.ts';

/**
 * Test whether `x` is the NaN value.
 *
 * @param x   the {@link MPFR} to inspect. Must pass {@link import('../core.ts').validate}.
 * @returns   `true` iff `x.kind === 'nan'`; `false` otherwise (including
 *            for normal, zero, and inf values).
 *
 * @mpfrName mpfr_nan_p
 *
 * @example
 *   mpfr_nan_p(NAN_VALUE);                     // true
 *   mpfr_nan_p(posInf(53n));                   // false
 *   mpfr_nan_p(posZero(53n));                  // false
 *   mpfr_nan_p(setD(3.14, 53n, 'RNDN').value); // false
 */
export function mpfr_nan_p(x: MPFR): boolean {
  // mpfr/src/isnan.c L27: `return MPFR_IS_NAN(x)`. The C macro inspects
  // the sentinel exponent `__MPFR_EXP_NAN`; our schema promotes that to
  // an explicit `kind` discriminant (see src/core.ts L30–L36), so the TS
  // equivalent is a single field comparison.
  return x.kind === 'nan';
}
