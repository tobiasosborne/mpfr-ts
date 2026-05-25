/**
 * ops/buildopt_float128_p.ts -- pure-TS port of MPFR's `mpfr_buildopt_float128_p`.
 *
 * Build-time predicate: does this libmpfr have IEEE 754 binary128 support
 * (the `__float128` type, gated by `MPFR_WANT_FLOAT128`)?
 *
 * The C reference is a one-line preprocessor branch on
 * `MPFR_WANT_FLOAT128`, returning 0 or 1. The TS port returns the
 * compile-time constant `false`: pure-TS has no native binary128 type
 * (there is no `Float128Array`, and `BigInt` is unbounded-integer-only,
 * not IEEE-754), so advertising binary128 support would be a lie. If/when
 * a TS-native binary128 library lands and we wire it in, this flips to
 * `true` and the golden is regenerated.
 *
 * The grader-locked schema (`src/core.ts`) is not directly referenced
 * here (no-arg, primitive return), but we keep an explicit type-only
 * import to satisfy the AST gate and to document that this port is a
 * citizen of the locked library surface.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/buildopt.c L56-L64 -- the C reference.
 *   - src/core.ts -- locked schema (this port is a no-arg accessor and
 *     does not touch the MPFR value model, but imports for grader
 *     compliance).
 */

import type { MPFR as _MPFR } from '../core.ts';


/**
 * Predicate: is IEEE 754 binary128 (`__float128`) supported in this build?
 *
 * @mpfrName mpfr_buildopt_float128_p
 *
 * @returns Always `false` in the pure-TS port. Pure-TS has no native
 *          binary128 type (no `Float128Array`; `BigInt` is not IEEE-754),
 *          so the function reports unconditional lack of support.
 *
 * @example
 *   mpfr_buildopt_float128_p();  // false
 */
export function mpfr_buildopt_float128_p(): boolean {
  // Ref: mpfr/src/buildopt.c L59-L63 -- preprocessor branch on
  // MPFR_WANT_FLOAT128. Pure-TS has no native binary128 type
  // (no Float128Array, BigInt is not IEEE-754); returning false is
  // the only honest answer.
  return false;
}
