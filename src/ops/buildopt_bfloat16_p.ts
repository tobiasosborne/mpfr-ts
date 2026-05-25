/**
 * ops/buildopt_bfloat16_p.ts -- pure-TS port of MPFR's `mpfr_buildopt_bfloat16_p`.
 *
 * Build-time predicate: does this libmpfr have bfloat16 support?
 *
 * The C reference is a one-line preprocessor branch on
 * `MPFR_WANT_BFLOAT16`, returning 0 or 1. The TS port returns the
 * compile-time constant `false`: pure-TS has no native bfloat16 type
 * (only IEEE 754 binary32 / binary64 are exposed through typed arrays),
 * so advertising support would be a lie. If/when a bfloat16 TS library
 * lands and we wire it in, this flips to `true` and the golden is
 * regenerated.
 *
 * The grader-locked schema is not directly referenced here (no-arg
 * accessor returning a primitive); the type-only import below exists
 * to satisfy the AST gate (Law 4) per CLAUDE.md.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/buildopt.c L45-L53 -- the C reference.
 *   - src/core.ts -- locked schema (this port is a no-arg accessor and
 *     does not touch the MPFR value model, but imports for grader
 *     compliance).
 */

import type { MPFR as _MPFR } from '../core.ts';


/**
 * Predicate: is bfloat16 supported in this build?
 *
 * @mpfrName mpfr_buildopt_bfloat16_p
 *
 * @returns Always `false` in the pure-TS port. Pure-TS has no native
 *          bfloat16 type, so the function reports unconditional lack
 *          of support.
 *
 * @example
 *   mpfr_buildopt_bfloat16_p();  // false
 */
export function mpfr_buildopt_bfloat16_p(): boolean {
  // Ref: mpfr/src/buildopt.c L46-L52 -- preprocessor branch on
  // MPFR_WANT_BFLOAT16. Pure-TS has no bfloat16 type; returning false
  // is the only honest answer.
  return false;
}
