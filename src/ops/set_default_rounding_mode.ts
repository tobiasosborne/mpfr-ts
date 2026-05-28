// The AST gate (Law 4) requires every misc-class port to import from
// src/core.ts even when the function signature has no MPFR-typed
// parameter. The import is type-only; no runtime values are needed.
import type { MPFR as _MPFR } from "../core.ts";

/**
 * mpfr/set_rnd.ts -- pure-TS port of MPFR's `mpfr_set_default_rounding_mode`.
 *
 * C signature:
 *
 *   void mpfr_set_default_rounding_mode(mpfr_rnd_t rnd)
 *
 * The C function stores `rnd` into the thread-global
 * `__gmpfr_default_rounding_mode` IFF `rnd >= MPFR_RNDN && rnd <
 * MPFR_RND_MAX` (i.e. rnd is in 0..5); otherwise it is a silent no-op
 * (the global is left unchanged). The TS surface takes `(rnd, prior)`
 * and returns the resulting code (`rnd` if accepted, `prior` if
 * rejected) -- a pure function with no observable global mutation.
 *
 * Integer codes (from this build's mpfr.h L100-L111):
 *   0 RNDN, 1 RNDZ, 2 RNDU, 3 RNDD, 4 RNDA, 5 RNDF,
 *   MPFR_RND_MAX == 6, retired RNDNA == -1.
 *
 * Ref: mpfr/src/set_rnd.c L27-L32 -- body: bounds check + store, else no-op.
 * Ref: /usr/include/mpfr.h L100-L111 -- mpfr_rnd_t enum with integer codes.
 */

/**
 * Set (query) the default MPFR rounding mode.
 *
 * @param rnd   Integer rounding-mode code to attempt to set (0..5 valid).
 * @param prior Previous/default rounding-mode code to return if `rnd` is
 *              rejected. Exposed as an explicit input so the function is
 *              deterministic with no hidden global / mutable state.
 * @returns     `rnd` if `0 <= rnd < 6`; otherwise `prior`.
 *
 * The returned number matches the raw integer codes from the C enum:
 * 0=RNDN, 1=RNDZ, 2=RNDU, 3=RNDD, 4=RNDA, 5=RNDF.
 */
export function mpfr_set_default_rounding_mode(
  rnd: number,
  prior: number,
): number {
  // Accepted range: 0..5 inclusive (MPFR_RNDN through MPFR_RNDF).
  // rnd < 0 or rnd >= 6 leaves the state at its prior value.
  // Ref: mpfr/src/set_rnd.c L27-L32 -- `rnd >= MPFR_RNDN && rnd < MPFR_RND_MAX`.
  if (rnd >= 0 && rnd < 6) {
    return rnd;
  }
  return prior;
}
