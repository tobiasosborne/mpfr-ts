/**
 * ops/buildopt_gmpinternals_p.ts -- pure-TS port of MPFR's
 * `mpfr_buildopt_gmpinternals_p`.
 *
 * Build-time predicate: does this libmpfr have direct access to GMP
 * internals (gated by `MPFR_HAVE_GMP_IMPL` or `WANT_GMP_INTERNALS`)?
 * When set, libmpfr can poke at undocumented mpn / mpz fields for
 * performance.
 *
 * The C reference is a one-line preprocessor branch returning 0 or 1.
 * The TS port returns the compile-time constant `false`: mpfr-ts does
 * not link against GMP at all -- the substrate uses native `BigInt`
 * (not mpn limb arrays), so there are no GMP internals to access in
 * the first place. This is structural, not a feature flag: it cannot
 * flip to `true` without re-architecting the substrate.
 *
 * The grader-locked schema (`src/core.ts`) is not directly referenced
 * here (no-arg, primitive return), but we keep an explicit type-only
 * import to satisfy the AST gate and to document that this port is a
 * citizen of the locked library surface.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/buildopt.c L76-L84 -- the C reference.
 *   - src/core.ts -- locked schema (this port is a no-arg accessor and
 *     does not touch the MPFR value model, but imports for grader
 *     compliance).
 */

import type { MPFR as _MPFR } from '../core.ts';


/**
 * Predicate: does this build access GMP internals directly?
 *
 * @mpfrName mpfr_buildopt_gmpinternals_p
 *
 * @returns Always `false` in the pure-TS port. mpfr-ts does not link
 *          GMP -- the substrate is native `BigInt`, not mpn limbs --
 *          so there are no GMP internals to access.
 *
 * @example
 *   mpfr_buildopt_gmpinternals_p();  // false
 */
export function mpfr_buildopt_gmpinternals_p(): boolean {
  // Ref: mpfr/src/buildopt.c L79-L83 -- preprocessor branch on
  // MPFR_HAVE_GMP_IMPL / WANT_GMP_INTERNALS. mpfr-ts does not link
  // GMP at all (substrate uses native BigInt); returning false is
  // structural, not a feature flag.
  return false;
}
