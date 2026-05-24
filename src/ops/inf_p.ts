/**
 * ops/inf_p.ts — pure-TS port of MPFR's `mpfr_inf_p`.
 *
 * "Is infinity?" predicate. Returns `true` iff `x` is `±Inf`, `false`
 * for any other kind (normal, zero, nan). Sign is not inspected — both
 * `+Inf` and `-Inf` match.
 *
 * C signature
 * -----------
 *
 *   int mpfr_inf_p(mpfr_srcptr x);
 *
 *   Returns non-zero iff `x` is `±Inf`, zero otherwise. The C body
 *   defers to the `MPFR_IS_INF(x)` macro (sentinel exponent
 *   `__MPFR_EXP_INF`). See mpfr/src/isinf.c L24–L28:
 *
 *     int (mpfr_inf_p) (mpfr_srcptr x) {
 *       return MPFR_IS_INF(x);
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_inf_p(x: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * C returns `int`; TS returns `boolean`. As with `mpfr_nan_p`, the
 * predicate never throws — "is infinity?" is well-defined for every
 * well-formed {@link MPFR} kind including NaN (returns `false`).
 *
 * Algorithm
 * ---------
 *
 * Single field comparison against the `'inf'` kind discriminant. The C
 * macro `MPFR_IS_INF` reads the sentinel exponent; ours reads `kind`
 * directly.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/isinf.c L24–L28 — the C reference.
 *   - src/core.ts L73 — `MPFRKind` (the `'inf'` discriminant).
 *   - src/ops/nan_p.ts — adjacent port; identical shape modulo kind.
 */

import type { MPFR } from '../core.ts';

/**
 * Test whether `x` is `+Inf` or `-Inf`.
 *
 * @param x   the {@link MPFR} to inspect. Must pass {@link import('../core.ts').validate}.
 * @returns   `true` iff `x.kind === 'inf'` (sign-agnostic); `false`
 *            otherwise (including for normal, zero, and NaN values).
 *
 * @mpfrName mpfr_inf_p
 *
 * @example
 *   mpfr_inf_p(posInf(53n));                   // true
 *   mpfr_inf_p(negInf(53n));                   // true  — sign-agnostic
 *   mpfr_inf_p(NAN_VALUE);                     // false
 *   mpfr_inf_p(posZero(53n));                  // false
 *   mpfr_inf_p(setD(3.14, 53n, 'RNDN').value); // false
 */
export function mpfr_inf_p(x: MPFR): boolean {
  // mpfr/src/isinf.c L27: `return MPFR_IS_INF(x)`. Same dispatch
  // collapse as `mpfr_nan_p`: the C sentinel-exponent check becomes a
  // `kind` discriminant comparison in TS.
  return x.kind === 'inf';
}
