/**
 * ops/mpfr_regular_p.ts — pure-TS port of MPFR's `mpfr_regular_p`.
 *
 * Surface-class predicate. Returns `true` iff `x` is a regular number —
 * i.e. finite, non-zero, and not NaN. Under the locked schema this is
 * exactly `kind === 'normal'`.
 *
 * C signature:
 *
 *   int mpfr_regular_p(mpfr_srcptr x)
 *
 * C body (one line):
 *
 *   return MPFR_IS_SINGULAR(x) == 0;
 *
 * where MPFR_IS_SINGULAR detects NaN, Inf, or zero via sentinel exponent
 * values (__MPFR_EXP_NAN, __MPFR_EXP_INF, __MPFR_EXP_ZERO).
 *
 * In the TypeScript surface the four-way `kind` discriminant makes this
 * trivial: a value is regular iff `kind === 'normal'`.
 *
 * Ref: mpfr/src/isregular.c L24-L28 — canonical one-line C body.
 * Ref: src/core.ts — MPFRKind discriminant; 'normal' is the regular case.
 *
 * @mpfrName mpfr_regular_p
 */

import type { MPFR } from "../core.ts";

/**
 * Returns `true` iff `x` is a regular (finite, non-zero, non-NaN) value.
 *
 * Mirrors `mpfr_regular_p` from MPFR: the return value is `true` iff
 * `MPFR_IS_SINGULAR(x) == 0`, i.e. `x` is neither NaN, infinity, nor zero.
 * Under the locked schema this is equivalent to `x.kind === 'normal'`.
 *
 * Ref: mpfr/src/isregular.c L24-L28
 * Ref: mpfr/src/mpfr-impl.h — MPFR_IS_SINGULAR macro checks sentinel exponents
 *
 * @param x Any MPFR value (NaN, Inf, zero, or normal).
 * @returns `true` iff `x.kind === 'normal'`.
 */
export function mpfr_regular_p(x: MPFR): boolean {
  // Ref: mpfr/src/isregular.c L27 — `return MPFR_IS_SINGULAR(x) == 0;`
  // MPFR_IS_SINGULAR returns nonzero for NaN, Inf, and zero (all non-normal).
  // Under our schema, kind === 'normal' is exactly the regular case.
  return x.kind === 'normal';
}
