/**
 * ops/cmp.ts — pure-TS port of MPFR's `mpfr_cmp`.
 *
 * Compare two {@link MPFR} values and return a signed comparison result
 * in `{-1, 0, +1}` — `-1` if `a < b`, `0` if `a == b`, `+1` if `a > b`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp(mpfr_srcptr op1, mpfr_srcptr op2);
 *
 *   Returns the sign of `op1 - op2`. Sets the erange flag and returns 0
 *   if either operand is NaN. See mpfr/src/cmp.c L100–L105: `mpfr_cmp`
 *   is just `mpfr_cmp3(op1, op2, +1)`; the underlying algorithm lives
 *   at mpfr/src/cmp.c L32–L98.
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmp(a: MPFR, b: MPFR): number;
 *
 *   - takes two immutable {@link MPFR} values from src/core.ts;
 *   - returns a plain JS `number` (∈ `{-1, 0, +1}`) — no Result wrapper
 *     because there is no rounding to attribute ternary to.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * MPFR's C surface signals NaN by setting an erange flag and returning
 * 0 — a silent "invalid comparison" channel that callers must remember
 * to test. The idiomatic TS port instead **throws** `MPFRError` with
 * code `'EDOMAIN'` on any NaN operand. The rationale is in CLAUDE.md
 * "Hallucination-risk callouts: NaN ≠ NaN":
 *
 *   > the idiomatic TS port should throw a documented MPFRRangeError,
 *   > never silently return 0.
 *
 * Callers that need the C semantics can wrap in a try/catch; callers
 * that don't get a loud failure instead of a silent wrong-zero. This
 * matches the precedent set by the other ops in `src/ops/` (set_d /
 * get_d / init2 all throw `MPFRError` rather than rely on erange).
 *
 * Algorithm
 * ---------
 *
 * Mirrors mpfr/src/cmp.c L32–L98 step-by-step. The C source structures
 * the dispatch around `MPFR_ARE_SINGULAR` (true if either operand is
 * NaN, Inf, or zero); the TS port flattens this into discrete `kind`
 * checks because the locked schema makes the discriminant explicit.
 *
 *   1. NaN: throw EDOMAIN. (C: erange + return 0; mpfr/src/cmp.c L43–L47.)
 *
 *   2. Both zero (regardless of sign): return 0. Signed zero is NOT
 *      ordered by mpfr_cmp — `+0 == -0` for comparison even though both
 *      are observably distinct under arithmetic. This is a documented
 *      MPFR quirk; see mpfr/src/cmp.c L57–L58:
 *
 *        else if (MPFR_IS_ZERO(b))
 *          return MPFR_IS_ZERO(c) ? 0 : -s;
 *
 *      (s here is +1 for the mpfr_cmp entry; the early-return reads
 *      "zero vs zero is equal, zero vs anything-nonzero is opposite the
 *      sign of the nonzero operand".)
 *
 *   3. Both Inf: return `a.sign - b.sign` clamped to {-1, 0, +1}. So
 *      `+Inf vs -Inf → +1`, `-Inf vs +Inf → -1`, same-sign infinities
 *      return 0. (mpfr/src/cmp.c L48–L54.)
 *
 *   4. One Inf:
 *        - if `a` is Inf: return `a.sign` (Inf magnitude dominates).
 *        - if `b` is Inf: return `-b.sign`.
 *      (mpfr/src/cmp.c L48–L56.)
 *
 *   5. One zero (the other normal):
 *        - if `a` is zero: return `-b.sign` (zero < positive, > negative).
 *        - if `b` is zero: return `a.sign`.
 *      (mpfr/src/cmp.c L57–L60, with `s=+1` substituted.)
 *
 *   6. Both normal:
 *        a. Sign differs: return `a.sign`. (mpfr/src/cmp.c L63–L64.)
 *        b. Same sign, exponent decides:
 *             a.exp > b.exp → +a.sign  (a larger in magnitude)
 *             a.exp < b.exp → -a.sign
 *           (mpfr/src/cmp.c L68–L73.)
 *        c. Sign and exponent both equal: compare mantissas. Because
 *           a.prec and b.prec may differ, we align both to a common
 *           width (max(a.prec, b.prec)) by left-shifting the
 *           lower-prec mantissa. The aligned bigints can then be
 *           compared directly; the sign of `(a_aligned - b_aligned)`
 *           multiplied by `a.sign` is the result.
 *
 *           Why the multiply by `a.sign`: at this point `a.sign === b.sign`,
 *           so the larger-magnitude operand is the one with the larger
 *           aligned mantissa. If both are positive, larger magnitude
 *           means larger value (+1); if both are negative, larger
 *           magnitude means smaller value (-1). That's exactly `a.sign`
 *           applied to the magnitude comparison.
 *
 *           (Mirrors mpfr/src/cmp.c L77–L97 — the limb-by-limb walk
 *           there is just an MSB-aligned multi-limb integer compare; we
 *           do the same in one bigint operation.)
 *
 * Same-value-different-prec
 * -------------------------
 *
 * Because the locked schema MSB-aligns the mantissa to its own `prec`,
 * the same value at two different precisions has two different `mant`
 * bigints. For example, `1.5` at prec=2 has `mant = 0b11 = 3`,
 * `exp=1`, and at prec=53 has `mant = 0b11 << 51 = 3 * 2^51`,
 * `exp=1`. Left-shifting the lower-prec mantissa by `(53 - 2) = 51`
 * bits gives `3 << 51`, equal to the prec=53 mant — so the alignment
 * step is what makes mixed-prec equality observable. The
 * `eval/reference_ports/broken/mpfr_cmp.ts` mutation skips this shift
 * and so reports such pairs as nonzero. Per CLAUDE.md PIL.3, that
 * broken port must score composite ≤ 0.5.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmp.c L32–L105 — the C reference (the `mpfr_cmp3` core
 *     and the `mpfr_cmp` entry that fixes `s=+1`).
 *   - src/core.ts — locked schema. The MPFR value model with explicit
 *     `kind` discriminant + signed zero/inf is what makes the dispatch
 *     direct here vs. the C source's bitmask probing.
 *   - src/internal/mpn/cmp.ts — the substrate-level limb compare. Note
 *     the iteration direction is OPPOSITE there (LSB-first vs MSB-first)
 *     because limbs are little-endian by index — though here we work in
 *     bigint, which handles the byte order implicitly.
 *   - CLAUDE.md "Hallucination-risk callouts": "NaN ≠ NaN" (we throw on
 *     NaN); "Signed zero is real" (preserved for arithmetic, but NOT
 *     for cmp — see step 2 above); "Rounding mode count is FIVE" (does
 *     not apply, no rounding here).
 */

import type { MPFR } from '../core.ts';
import { MPFRError, validate } from '../core.ts';

/**
 * Compare two {@link MPFR} values. Returns `-1` if `a < b`, `0` if `a
 * == b`, `+1` if `a > b`. Signed zero is **not** ordered: `+0 == -0`
 * for the purposes of this comparison.
 *
 * @param a left operand. Must pass {@link validate}.
 * @param b right operand. Must pass {@link validate}.
 * @returns `-1`, `0`, or `+1`.
 * @throws {MPFRError} `EDOMAIN` if either operand has `kind === 'nan'`.
 *   This diverges from the C reference, which sets the erange flag and
 *   returns 0 — see the file docstring "Divergence from C → TS".
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation (malformed shape; should not occur for values returned
 *   by other ops in this library).
 *
 * @mpfrName mpfr_cmp
 */
export function mpfr_cmp(a: MPFR, b: MPFR): number {
  // Structural validation. Inputs to ops in this library are expected
  // to pass validate() already (other ops produce values that pass
  // validate by construction); the re-check here catches malformed
  // JSON-decoded inputs at the grader boundary and surfaces them as
  // EPREC rather than a downstream silent miscompare. Ref: src/core.ts
  // §validate.
  validate(a);
  validate(b);

  // Step 1: NaN. Divergence from C — we throw EDOMAIN rather than
  // returning 0 + setting an erange flag. The throw is loud and the
  // caller can wrap in try/catch if C semantics are desired. Ref:
  // mpfr/src/cmp.c L43–L47 (C: ERANGE + return 0).
  if (a.kind === 'nan' || b.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp: NaN operand (a.kind=${a.kind}, b.kind=${b.kind})`,
    );
  }

  // Step 2: both zero. Returns 0 regardless of sign — signed zero is
  // NOT ordered by mpfr_cmp. Ref: mpfr/src/cmp.c L57–L58.
  if (a.kind === 'zero' && b.kind === 'zero') {
    return 0;
  }

  // Step 3: both Inf. Same sign → 0; cross sign → sign of `a`. Ref:
  // mpfr/src/cmp.c L48–L54.
  if (a.kind === 'inf' && b.kind === 'inf') {
    if (a.sign === b.sign) return 0;
    return a.sign; // a is +Inf, b is -Inf → +1; or mirror.
  }

  // Step 4: exactly one operand is Inf. (We already handled both-Inf
  // above; this branch is "exactly one is Inf, the other is finite".)
  // The infinite operand dominates in magnitude. Ref: mpfr/src/cmp.c
  // L48–L56.
  if (a.kind === 'inf') {
    return a.sign;
  }
  if (b.kind === 'inf') {
    return -b.sign as 1 | -1;
  }

  // Step 5: exactly one operand is zero (the other necessarily normal,
  // since we've eliminated NaN above and Inf in step 4). Ref:
  // mpfr/src/cmp.c L57–L60 with s=+1.
  if (a.kind === 'zero') {
    // a is ±0, b is normal. Return -b.sign (zero is less than positive,
    // greater than negative). Cast to 1|-1 to satisfy the return type.
    return -b.sign as 1 | -1;
  }
  if (b.kind === 'zero') {
    // a is normal, b is ±0. Return a.sign.
    return a.sign;
  }

  // Step 6: both 'normal'. (The remaining case after steps 1–5: every
  // singular combination has been handled. The switch-exhaustiveness
  // check would require a final `default` but we encode the invariant
  // directly via the assertion below.)
  if (a.kind !== 'normal' || b.kind !== 'normal') {
    // Unreachable given the chain of guards above. The throw is here so
    // a future refactor that loosens the dispatch surfaces here rather
    // than silently producing a wrong answer.
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp: internal invariant violated — non-normal pair reached normal-vs-normal branch (a.kind=${a.kind}, b.kind=${b.kind})`,
    );
  }

  // Step 6a: signs differ — positive > negative. Ref: mpfr/src/cmp.c
  // L63–L64.
  if (a.sign !== b.sign) {
    return a.sign;
  }

  // Step 6b: same sign, exponents decide. Ref: mpfr/src/cmp.c L68–L73.
  // For positive a/b: larger exp → larger value → return +1 (a.sign).
  // For negative a/b: larger exp → larger magnitude → MORE negative →
  // return -1 (a.sign × -1, which is -a.sign... but a.sign here is -1
  // so -a.sign = +1 = wait. Let's re-derive: a.sign multiplies the
  // magnitude comparison. magnitude(a) > magnitude(b) gives +1 raw;
  // sign-aware result is +1 * a.sign = a.sign. So same-sign-larger-exp
  // returns a.sign.).
  if (a.exp > b.exp) return a.sign;
  if (a.exp < b.exp) return -a.sign as 1 | -1;

  // Step 6c: same sign, same exponent — mantissa decides after
  // alignment to a common width. The MSB-aligned mantissas can have
  // different bit-widths (one per its own prec); shift the
  // lower-prec one up by the difference so both are at width
  // max(a.prec, b.prec). Then compare directly. Multiply by a.sign to
  // turn the magnitude comparison into the signed comparison.
  //
  // Why the shift (vs right-shifting the higher-prec one): we never
  // want to discard bits. The lower-prec mantissa, when MSB-aligned
  // to its own prec, has zeros below its LSB position when viewed at
  // the higher prec — left-shifting by (max_prec - its_prec) makes
  // those implicit zeros explicit. The higher-prec mantissa may have
  // non-zero bits in that lower region (case (28) in golden_driver.c);
  // those bits CANNOT be discarded without changing the answer.
  //
  // Performance: one bigint subtract or two bigint compares. For
  // typical precs (53-256) bigint ops are tens of nanoseconds — comfortably
  // inside the arithmetic-class 50ms budget.
  //
  // Ref: mpfr/src/cmp.c L77–L97 — limb-by-limb MSB-first walk;
  // equivalent to a single multi-precision integer compare on the
  // aligned mantissas.
  const maxPrec = a.prec > b.prec ? a.prec : b.prec;
  const aAligned = a.mant << (maxPrec - a.prec);
  const bAligned = b.mant << (maxPrec - b.prec);
  if (aAligned > bAligned) return a.sign;
  if (aAligned < bAligned) return -a.sign as 1 | -1;
  return 0;
}
