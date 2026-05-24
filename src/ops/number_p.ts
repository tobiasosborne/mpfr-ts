/**
 * ops/number_p.ts — pure-TS port of MPFR's `mpfr_number_p`.
 *
 * "Is an ordinary (finite) number?" predicate. Returns `true` iff `x`
 * is finite — i.e. `kind === 'normal'` or `kind === 'zero'` — and
 * `false` for the two singular non-finite kinds (`'inf'`, `'nan'`).
 *
 * This matches the C macro `MPFR_IS_FP(x)` which the C reference uses;
 * `IS_FP` evaluates true for ordinary floating-point values and false
 * for ±Inf and NaN.
 *
 * C signature
 * -----------
 *
 *   int mpfr_number_p(mpfr_srcptr x);
 *
 *   Returns non-zero iff `x` is finite (zero OR normal). See
 *   mpfr/src/isnum.c L24–L28:
 *
 *     int mpfr_number_p (mpfr_srcptr x) {
 *       return MPFR_IS_FP(x);
 *     }
 *
 *   `MPFR_IS_FP(x)` is defined in mpfr-impl.h as roughly
 *   `(! MPFR_IS_NAN(x) && ! MPFR_IS_INF(x))`. Note that zero counts as
 *   finite — there is no separate "subnormal" kind to consider.
 *
 * TS signature
 * ------------
 *
 *   mpfr_number_p(x: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * C returns `int`; TS returns `boolean`. Never throws.
 *
 * Note that this is intentionally NOT `kind === 'normal'`: per the
 * MPFR manual, `mpfr_number_p` returns true for any *finite* value,
 * which includes signed zero. The companion `mpfr_regular_p`
 * predicate is the one that excludes zero — but we don't port it in
 * this batch.
 *
 * Algorithm
 * ---------
 *
 * Two-way disjunction over the kind discriminant. Equivalently:
 * `kind !== 'inf' && kind !== 'nan'`. We use the positive form for
 * symmetry with the underlying value model (finite kinds are
 * enumerated; non-finite kinds are exceptions).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/isnum.c L24–L28 — the C reference.
 *   - mpfr/src/mpfr-impl.h §"MPFR_IS_FP" — the macro definition.
 *   - src/core.ts L300–L302 — equivalent `isFinite(x)` helper in core.
 *   - src/core.ts L73 — `MPFRKind`.
 */

import type { MPFR } from '../core.ts';

/**
 * Test whether `x` is a finite (ordinary) floating-point value.
 *
 * @param x   the {@link MPFR} to inspect. Must pass {@link import('../core.ts').validate}.
 * @returns   `true` iff `x.kind === 'normal' || x.kind === 'zero'`;
 *            `false` for `±Inf` and `NaN`.
 *
 * @mpfrName mpfr_number_p
 *
 * @example
 *   mpfr_number_p(setD(3.14, 53n, 'RNDN').value); // true
 *   mpfr_number_p(posZero(53n));                  // true  — zero is finite
 *   mpfr_number_p(negZero(53n));                  // true
 *   mpfr_number_p(posInf(53n));                   // false
 *   mpfr_number_p(negInf(53n));                   // false
 *   mpfr_number_p(NAN_VALUE);                     // false
 */
export function mpfr_number_p(x: MPFR): boolean {
  // mpfr/src/isnum.c L27: `return MPFR_IS_FP(x)`, where MPFR_IS_FP
  // means "not NaN AND not Inf". We enumerate the positive cases
  // ('normal' and 'zero') rather than negate the two exceptional ones
  // — both are correct; the positive form makes the docstring's
  // "ordinary floating-point" framing match the body.
  return x.kind === 'normal' || x.kind === 'zero';
}
