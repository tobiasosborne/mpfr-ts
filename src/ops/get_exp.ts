/**
 * ops/get_exp.ts — pure-TS port of MPFR's `mpfr_get_exp`.
 *
 * Return the unbiased base-2 exponent of a finite non-zero MPFR value.
 *
 * C signature
 * -----------
 *
 *   mpfr_exp_t mpfr_get_exp(mpfr_srcptr x);
 *
 *   Returns MPFR_EXP(x), guarded by an assertion that x is a "pure FP"
 *   value (i.e. a finite non-zero normal number). If x is NaN, ±∞, or ±0,
 *   the C assertion fires and the program aborts.
 *
 *   Ref: mpfr/src/get_exp.c L24–L30:
 *
 *     #undef mpfr_get_exp
 *     mpfr_exp_t
 *     mpfr_get_exp (mpfr_srcptr x)
 *     {
 *       MPFR_ASSERTN(MPFR_IS_PURE_FP(x));
 *       return MPFR_EXP(x);  // do not use MPFR_GET_EXP of course...
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_get_exp(x: MPFR): bigint;
 *
 *   - Takes a single immutable {@link MPFR} from `src/core.ts`.
 *   - Returns `x.exp` as a `bigint` when `x.kind === 'normal'`.
 *   - Throws `MPFRError('EDOMAIN')` for `kind === 'zero' | 'inf' | 'nan'`
 *     (mirroring the C ASSERTN abort for non-PURE_FP inputs).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The C implementation unconditionally aborts via MPFR_ASSERTN on any
 * non-normal input; TS turns this into a documented MPFRError('EDOMAIN')
 * throw. This gives callers a catchable error at the TS boundary rather
 * than an uncatchable assertion failure, which is more useful in practice
 * (same pattern used in src/ops/cmp.ts for NaN inputs).
 *
 * The TS schema stores `exp=0n` as a sentinel for zero/inf/nan (per
 * src/core.ts validate). We explicitly throw rather than returning 0n
 * to surface the domain-error contract clearly — a silent 0n return would
 * mask the misuse.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/get_exp.c L24–L30 — the C reference body.
 *   - mpfr/src/mpfr-impl.h — MPFR_IS_PURE_FP, MPFR_EXP macros.
 *   - src/core.ts L113–L135 — locked MPFR shape; exp=0n for non-normal kinds.
 *   - src/core.ts §MPFRError, EDOMAIN — domain-error class for non-recoverable input.
 *   - eval/functions/mpfr_get_exp/spec.json — function-specific contract.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN != NaN" — EDOMAIN throw pattern.
 */

import type { MPFR } from "../core.ts";
import { MPFRError } from "../core.ts";

/**
 * Return the unbiased base-2 exponent of `x`.
 *
 * The exponent `e` satisfies `|x| ∈ [2^(e-1), 2^e)` for normal values.
 * This is the same convention MPFR uses externally — `MPFR_EXP(x)` in C.
 *
 * @mpfrName mpfr_get_exp
 *
 * @param x  The {@link MPFR} value to inspect. Must be a finite non-zero
 *           normal value (`kind === 'normal'`).
 * @returns  `x.exp` as a `bigint`.
 *
 * @throws {MPFRError} `EDOMAIN` if `x` is not a normal (finite non-zero)
 *         value — i.e. if `x.kind` is `'zero'`, `'inf'`, or `'nan'`. This
 *         mirrors the C-side `MPFR_ASSERTN(MPFR_IS_PURE_FP(x))` which aborts
 *         the program on the same inputs.
 *
 * @example
 *   mpfr_get_exp(setD(1.0, 53n, 'RNDN').value);  // 1n  (1.0 is in [2^0, 2^1))
 *   mpfr_get_exp(setD(4.0, 53n, 'RNDN').value);  // 3n  (4.0 is in [2^2, 2^3))
 *   mpfr_get_exp(posZero(53n));                   // throws MPFRError EDOMAIN
 *   mpfr_get_exp(posInf(53n));                    // throws MPFRError EDOMAIN
 *   mpfr_get_exp(NAN_VALUE);                      // throws MPFRError EDOMAIN
 */
export function mpfr_get_exp(x: MPFR): bigint {
  // Ref: mpfr/src/get_exp.c L28 — `MPFR_ASSERTN(MPFR_IS_PURE_FP(x));`
  // MPFR_IS_PURE_FP checks that x is neither NaN, ±0, nor ±inf — i.e. it
  // must be a normal (finite non-zero) number. We throw EDOMAIN to surface
  // the same domain error at the TS boundary rather than aborting.
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_get_exp: argument must be a finite non-zero value (MPFR_IS_PURE_FP), got kind='${x.kind}'`,
    );
  }

  // Ref: mpfr/src/get_exp.c L29 — `return MPFR_EXP(x);`
  // MPFR_EXP(x) expands to `((x)->_mpfr_exp)`, directly reading the exponent
  // field. In the TS schema this is `x.exp`, a bigint field on the immutable
  // MPFR record. For kind='normal', x.exp is the unbiased base-2 exponent
  // such that |x| ∈ [2^(exp-1), 2^exp).
  return x.exp;
}
