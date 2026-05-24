/**
 * ops/zero_p.ts — pure-TS port of MPFR's `mpfr_zero_p`.
 *
 * "Is zero?" predicate. Returns `true` iff `x` is `±0`, `false` for any
 * other kind (normal, inf, nan). Sign is not inspected — both `+0` and
 * `-0` match. Note that signed zero is observable in *arithmetic*
 * (`add(+0, -0, p, 'RNDD') === -0`), but `zero_p` collapses the two
 * into a single yes/no — they're the same kind.
 *
 * C signature
 * -----------
 *
 *   int mpfr_zero_p(mpfr_srcptr x);
 *
 *   Returns non-zero iff `x` is `±0`, zero otherwise. The C body defers
 *   to the `MPFR_IS_ZERO(x)` macro (sentinel exponent
 *   `__MPFR_EXP_ZERO`). See mpfr/src/iszero.c L24–L28:
 *
 *     int (mpfr_zero_p) (mpfr_srcptr x) {
 *       return MPFR_IS_ZERO(x);
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_zero_p(x: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * C returns `int`; TS returns `boolean`. Never throws.
 *
 * Algorithm
 * ---------
 *
 * Single field comparison against the `'zero'` kind discriminant.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/iszero.c L24–L28 — the C reference.
 *   - src/core.ts L73 — `MPFRKind` (the `'zero'` discriminant).
 *   - src/core.ts L90 — `Sign` doc on signed zero ("observable" in
 *     arithmetic, but `zero_p` doesn't distinguish — see above).
 */

import type { MPFR } from '../core.ts';

/**
 * Test whether `x` is `+0` or `-0`.
 *
 * @param x   the {@link MPFR} to inspect. Must pass {@link import('../core.ts').validate}.
 * @returns   `true` iff `x.kind === 'zero'` (sign-agnostic); `false`
 *            otherwise (including for normal, inf, and NaN values).
 *
 * @mpfrName mpfr_zero_p
 *
 * @example
 *   mpfr_zero_p(posZero(53n));                  // true
 *   mpfr_zero_p(negZero(53n));                  // true  — sign-agnostic
 *   mpfr_zero_p(NAN_VALUE);                     // false
 *   mpfr_zero_p(posInf(53n));                   // false
 *   mpfr_zero_p(setD(3.14, 53n, 'RNDN').value); // false
 */
export function mpfr_zero_p(x: MPFR): boolean {
  // mpfr/src/iszero.c L27: `return MPFR_IS_ZERO(x)`. Same dispatch
  // collapse as the sibling predicates.
  return x.kind === 'zero';
}
