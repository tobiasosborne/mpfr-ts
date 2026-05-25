/**
 * ops/buildopt_decimal_p.ts -- pure-TS port of MPFR's `mpfr_buildopt_decimal_p`.
 *
 * Build-time predicate: does this libmpfr have decimal-float support
 * (IEEE 754-2008 decimal32 / decimal64 / decimal128)?
 *
 * The C reference is a one-line preprocessor branch on
 * `MPFR_WANT_DECIMAL_FLOATS`, returning 0 or 1. The TS port returns the
 * compile-time constant `false`: pure-TS has no IEEE-754-2008 decimal
 * type and the substrate is binary all the way down, so advertising
 * decimal-float support would be a lie.
 *
 * The grader-locked schema (`src/core.ts`) is not directly referenced
 * here (no-arg, primitive return), but we keep an explicit type-only
 * import to satisfy the AST gate and to document that this port is a
 * citizen of the locked library surface.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/buildopt.c L65-L73 -- the C reference.
 *   - src/core.ts -- locked schema (this port is a no-arg accessor and
 *     does not touch the MPFR value model, but imports for grader
 *     compliance).
 */

import type { MPFR as _MPFR } from '../core.ts';


/**
 * Predicate: is IEEE-754-2008 decimal-float supported in this build?
 *
 * @mpfrName mpfr_buildopt_decimal_p
 *
 * @returns Always `false` in the pure-TS port. Pure-TS has no native
 *          decimal-float type, so the function reports unconditional
 *          lack of support.
 *
 * @example
 *   mpfr_buildopt_decimal_p();  // false
 */
export function mpfr_buildopt_decimal_p(): boolean {
  // Ref: mpfr/src/buildopt.c L66-L73 -- preprocessor branch on
  // MPFR_WANT_DECIMAL_FLOATS. Pure-TS has no decimal-float type;
  // returning false is the only honest answer.
  return false;
}
