/**
 * ops/signbit.ts — pure-TS port of MPFR's `mpfr_signbit`.
 *
 * "Is the sign bit set?" predicate. Returns `true` iff `x.sign === -1`
 * — i.e. the value is negative (or a `-0` / `-Inf`). Defined for every
 * MPFR kind including NaN; per the locked schema (`src/core.ts` L83–L85)
 * our NaN canonicalises to `sign === 1`, so `signbit(NaN) === false`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_signbit(mpfr_srcptr x);
 *
 *   Returns non-zero iff the sign bit is set. The C body defers to
 *   `MPFR_IS_NEG(x)` (which reads `_mpfr_sign < 0`). See
 *   mpfr/src/signbit.c L24–L29:
 *
 *     int mpfr_signbit (mpfr_srcptr x) {
 *       return MPFR_IS_NEG(x);
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_signbit(x: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * C returns `int`; TS returns `boolean`. The C function is documented
 * as defined for every kind including NaN; in C, libmpfr's NaN can
 * carry either sign depending on its provenance, so `mpfr_signbit(NaN)`
 * is `0` or `1` depending on the originating op.
 *
 * The TS locked schema canonicalises every NaN to `sign === 1` (see
 * src/core.ts L83–L85, L243–L249: `NAN_VALUE` has `sign: 1`). The
 * downstream consequence is that `signbit(NaN) === false` *every time*
 * in the TS port — there is no negative-NaN. The golden master honours
 * this by emitting `false` for NaN cases (the C side computes its NaN
 * via `mpfr_set_nan`, which initialises the sign positive, so the C
 * golden and TS port agree on the answer for this canonical case).
 *
 * Algorithm
 * ---------
 *
 * One-line: read the `sign` field.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/signbit.c L24–L29 — the C reference.
 *   - src/core.ts L83–L85 — Sign doc (NaN convention).
 *   - src/core.ts L243–L249 — `NAN_VALUE` constant (sign: 1).
 *   - mpfr/src/mpfr.h L195–L196 — `mpfr_sign_t` is int; signbit
 *     reads `_mpfr_sign` directly.
 */

import type { MPFR } from '../core.ts';

/**
 * Test whether the sign bit of `x` is set (i.e. `x` is negative).
 *
 * @param x   the {@link MPFR} to inspect. Must pass {@link import('../core.ts').validate}.
 * @returns   `true` iff `x.sign === -1`; `false` otherwise. Defined for
 *            every kind; NaN always returns `false` (the TS schema
 *            canonicalises NaN to `sign === 1`).
 *
 * @mpfrName mpfr_signbit
 *
 * @example
 *   mpfr_signbit(setD(-1.5,  53n, 'RNDN').value); // true
 *   mpfr_signbit(setD( 1.5,  53n, 'RNDN').value); // false
 *   mpfr_signbit(negZero(53n));                   // true  — signed zero
 *   mpfr_signbit(posZero(53n));                   // false
 *   mpfr_signbit(negInf(53n));                    // true
 *   mpfr_signbit(posInf(53n));                    // false
 *   mpfr_signbit(NAN_VALUE);                      // false (TS NaN.sign === 1)
 */
export function mpfr_signbit(x: MPFR): boolean {
  // mpfr/src/signbit.c L28: `return MPFR_IS_NEG(x)`. MPFR_IS_NEG reads
  // `_mpfr_sign < 0`; our schema stores sign as `1 | -1` directly, so
  // the equivalent is `x.sign === -1`.
  return x.sign === -1;
}
