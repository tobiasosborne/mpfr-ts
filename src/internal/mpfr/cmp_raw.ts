/**
 * internal/mpfr/cmp_raw.ts — non-throwing MPFR-level comparison core.
 *
 * Pure-TS port of MPFR's mpfr_cmp3 dispatch (mpfr/src/cmp.c L32–L98) with
 * one critical behavioural change versus the public {@link import('../../ops/cmp.ts').mpfr_cmp}
 * surface: NaN does **not** throw, it returns `null`. This is the substrate
 * helper that both the throwing `mpfr_cmp` and the non-throwing predicate
 * family (`mpfr_less_p` / `mpfr_greater_p` / etc.) share.
 *
 * Why factor this out
 * -------------------
 *
 * The MPFR C reference uses one helper (`mpfr_cmp3`) and dispatches all
 * predicates through it (mpfr/src/comparisons.c L39–L73), each with its
 * own NaN-vs-non-NaN guard:
 *
 *   int mpfr_less_p(x, y) {
 *     return MPFR_IS_NAN(x) || MPFR_IS_NAN(y) ? 0 : (mpfr_cmp(x, y) < 0);
 *   }
 *
 * The TS port can't compose that pattern directly because the public
 * `mpfr_cmp` throws on NaN (a documented divergence — see src/ops/cmp.ts
 * §"Divergence from C → TS"). Wrapping every predicate in a try/catch is
 * ugly and slow; factoring out the non-throwing core is the clean fix.
 *
 * Result domain
 * -------------
 *
 *   - `compareMPFR(a, b)` returns:
 *     - `null`  if either operand has `kind === 'nan'` (unordered).
 *     - `-1`    if `a < b`.
 *     - `0`     if `a == b`. (Signed zero is NOT ordered — `+0 == -0`.)
 *     - `+1`    if `a > b`.
 *
 * Algorithm
 * ---------
 *
 * Step-by-step mirror of mpfr/src/cmp.c L32–L98 with the locked-schema
 * dispatch from src/ops/cmp.ts:
 *
 *   1. NaN: return `null`. (C: ERANGE + return 0; not applicable here.)
 *   2. Both zero (any sign): return 0. (mpfr/src/cmp.c L57–L58.)
 *   3. Both Inf: same sign → 0; cross sign → a.sign.
 *      (mpfr/src/cmp.c L48–L54.)
 *   4. Exactly one Inf: the Inf magnitude dominates.
 *   5. Exactly one zero (the other normal): the nonzero operand decides.
 *   6. Both normal:
 *      a. signs differ → return a.sign (positive > negative).
 *      b. same sign, exp decides — larger exp → larger magnitude, signed.
 *      c. same sign, same exp — align mantissas to max(prec) and compare.
 *
 * The MSB-aligned mantissa compare with prec-difference left-shift is the
 * load-bearing step (see src/ops/cmp.ts §"Same-value-different-prec").
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmp.c L32–L98 — the C reference (mpfr_cmp3).
 *   - mpfr/src/comparisons.c L39–L73 — predicates' "NaN → 0" wrapper
 *     pattern.
 *   - src/ops/cmp.ts — public throwing surface, now delegated through here.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — rationale for
 *     keeping NaN handling at the boundary, not inside the core.
 */

import type { MPFR } from '../../core.ts';
import { MPFRError, validate } from '../../core.ts';

/**
 * Result domain of {@link compareMPFR}. `null` is the unordered (NaN)
 * sentinel; the three numeric values are the ordered outcomes.
 */
export type CmpResult = -1 | 0 | 1 | null;

/**
 * Compare two MPFR values, returning a signed result in {-1, 0, +1} or
 * `null` if either operand is NaN. Pure, deterministic, no I/O.
 *
 * This is the SHARED core for both `src/ops/cmp.ts` (throws on NaN) and
 * the predicate family in `src/ops/{less,greater,lessequal,greaterequal,
 * equal}_p.ts` (return false on NaN). The dispatch logic is identical to
 * mpfr/src/cmp.c L32–L98 / src/ops/cmp.ts — the only behavioural change
 * is the NaN path: this function returns `null` rather than throwing.
 *
 * Both inputs are structurally validated; malformed shapes surface as
 * `MPFRError('EPREC', ...)` (the same boundary check the public surface
 * applies). NaN is treated as a valid input here — `null` is returned
 * without diagnostic. Callers that need a domain-error throw (mpfr_cmp)
 * must check for `null` and translate.
 *
 * @param a left operand. Must pass {@link validate}.
 * @param b right operand. Must pass {@link validate}.
 * @returns `null` if either operand is NaN; otherwise `-1`, `0`, or `+1`.
 * @throws {MPFRError} `EPREC` if either operand fails structural validation.
 */
export function compareMPFR(a: MPFR, b: MPFR): CmpResult {
  // Structural validation at the trust boundary. Both the public throwing
  // mpfr_cmp and the predicate family will have already validated their
  // own inputs, but doing it here as well costs nothing in the common
  // path (already-valid values short-circuit through the kind switch)
  // and catches a malformed value that bypassed an earlier guard.
  // Matches the pattern in src/ops/cmp.ts L162–L163.
  validate(a);
  validate(b);

  // Step 1: NaN. Return null (unordered) — unlike mpfr_cmp, no throw.
  // Ref: mpfr/src/comparisons.c L42 — predicates short-circuit NaN to 0.
  // Our `null` is a strict-mode marker the caller translates; `0` would
  // be ambiguous with "equal".
  if (a.kind === 'nan' || b.kind === 'nan') {
    return null;
  }

  // Step 2: both zero. Returns 0 regardless of sign — signed zero is NOT
  // ordered by mpfr_cmp. Ref: mpfr/src/cmp.c L57–L58.
  if (a.kind === 'zero' && b.kind === 'zero') {
    return 0;
  }

  // Step 3: both Inf. Same sign → 0; cross sign → sign of `a`. Ref:
  // mpfr/src/cmp.c L48–L54.
  if (a.kind === 'inf' && b.kind === 'inf') {
    if (a.sign === b.sign) return 0;
    return a.sign;
  }

  // Step 4: exactly one operand is Inf. Ref: mpfr/src/cmp.c L48–L56.
  if (a.kind === 'inf') {
    return a.sign;
  }
  if (b.kind === 'inf') {
    return -b.sign as 1 | -1;
  }

  // Step 5: exactly one operand is zero. Ref: mpfr/src/cmp.c L57–L60.
  if (a.kind === 'zero') {
    return -b.sign as 1 | -1;
  }
  if (b.kind === 'zero') {
    return a.sign;
  }

  // Step 6: both 'normal' — remaining case after steps 1–5.
  if (a.kind !== 'normal' || b.kind !== 'normal') {
    // Unreachable given the chain of guards above. The throw is here so
    // a future refactor that loosens the dispatch surfaces here rather
    // than silently producing a wrong answer.
    throw new MPFRError(
      'EPREC',
      `compareMPFR: internal invariant violated — non-normal pair reached normal-vs-normal branch (a.kind=${a.kind}, b.kind=${b.kind})`,
    );
  }

  // Step 6a: signs differ — positive > negative. Ref: mpfr/src/cmp.c L63–L64.
  if (a.sign !== b.sign) {
    return a.sign;
  }

  // Step 6b: same sign, exponents decide. Ref: mpfr/src/cmp.c L68–L73.
  if (a.exp > b.exp) return a.sign;
  if (a.exp < b.exp) return -a.sign as 1 | -1;

  // Step 6c: same sign, same exponent — mantissa decides after alignment
  // to a common width max(a.prec, b.prec). See src/ops/cmp.ts L266–L271
  // for the full rationale; the load-bearing point is that lower-prec
  // mantissas are MSB-aligned to their OWN prec, so a same-value pair
  // across different precs has unequal raw mant bigints — left-shifting
  // the lower-prec operand by (max_prec - its_prec) makes the implicit
  // zero tail explicit and the compare correct.
  //
  // Ref: mpfr/src/cmp.c L77–L97 — limb-by-limb MSB-first walk, which is
  // a multi-precision integer compare on the aligned mantissas.
  const maxPrec = a.prec > b.prec ? a.prec : b.prec;
  const aAligned = a.mant << (maxPrec - a.prec);
  const bAligned = b.mant << (maxPrec - b.prec);
  if (aAligned > bAligned) return a.sign;
  if (aAligned < bAligned) return -a.sign as 1 | -1;
  return 0;
}
