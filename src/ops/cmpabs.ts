/**
 * ops/cmpabs.ts — pure-TS port of MPFR's `mpfr_cmpabs`.
 *
 * Compare the absolute values of two {@link MPFR} operands and return a
 * signed result in `{-1, 0, +1}` — `-1` if `|b| < |c|`, `0` if
 * `|b| = |c|`, `+1` if `|b| > |c|`. Signs of the operands are ignored.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmpabs(mpfr_srcptr b, mpfr_srcptr c);
 *
 *   Returns positive if |b| > |c|, 0 if |b| = |c|, negative if
 *   |b| < |c|. Sets the erange flag and returns 0 if either operand is
 *   NaN. See mpfr/src/cmpabs.c L30–L86.
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmpabs(b: MPFR, c: MPFR): number;
 *
 *   Returns a plain JS `number` ∈ `{-1, 0, +1}` — no Result wrapper
 *   since there is no rounding to attribute a ternary flag to. The
 *   return is normalised to {-1, 0, +1} at the function boundary
 *   (C may technically return any non-zero integer for the > / <
 *   cases but we normalise for the TS surface per spec.json).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The C reference sets the erange flag and returns 0 on NaN inputs —
 * a silent invalid-comparison channel. The idiomatic TS port instead
 * **throws** `MPFRError('EDOMAIN', ...)` on any NaN operand, matching
 * the precedent set by `src/ops/cmp.ts`. This keeps the contract loud
 * rather than silently wrong. See CLAUDE.md "Hallucination-risk
 * callouts: NaN ≠ NaN".
 *
 * Algorithm
 * ---------
 *
 * Mirrors mpfr/src/cmpabs.c L30–L86 step-by-step, adapted for the
 * locked-schema MPFR value type. Because signs are stripped, the
 * dispatch simplifies vs the signed `mpfr_cmp`:
 *
 *   1. NaN: throw EDOMAIN. (C: ERANGE + return 0; mpfr/src/cmpabs.c L39–L42.)
 *
 *   2. Inf vs Inf: both are infinite magnitude → return 0. Inf vs any
 *      finite → 1. (mpfr/src/cmpabs.c L43–L44: `! MPFR_IS_INF(c)`.)
 *
 *   3. Finite vs Inf: return -1. (mpfr/src/cmpabs.c L45–L46.)
 *
 *   4. c is zero (b may be zero or normal):
 *        - c is zero, b is normal → return 1 (normal > zero magnitude).
 *        - c is zero, b is zero  → return 0 (both zero magnitude).
 *      (mpfr/src/cmpabs.c L47–L48: `! MPFR_IS_ZERO(b)`.)
 *
 *   5. b is zero, c is normal: return -1. (mpfr/src/cmpabs.c L49–L50.)
 *
 *   6. Both normal:
 *      a. Exponents decide — larger exp → larger magnitude. (L54–L57.)
 *      b. Equal exponents — compare mantissas after MSB-alignment to
 *         max(b.prec, c.prec) so cross-prec same-value pairs are equal.
 *         (L59–L84: the C limb-by-limb MSB-first walk is equivalent to
 *         comparing MSB-aligned bigints.)
 *
 * The limb-level detail in the C source (the two cleanup loops for
 * unequal limb counts) corresponds to the "trailing implicit zeros" of
 * the lower-prec mantissa when left-shifted: left-shifting by
 * (maxPrec - prec) fills in exactly those trailing zeros, so the single
 * bigint comparison subsumes all three C loops.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmpabs.c L30–L86 — the C reference body.
 *   - src/ops/cmp.ts — sibling signed comparison; same dispatch shape
 *     but with sign-awareness in the normal-vs-normal case.
 *   - src/internal/mpfr/cmp_raw.ts — shared signed compare core;
 *     cmpabs does NOT call this (it would re-introduce sign into the
 *     dispatch) but the mantissa-alignment technique is identical.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — domain-error
 *     throw vs silent zero.
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *     zero kinds still compared by magnitude (both zero → 0, not by sign).
 */

import type { MPFR } from '../core.ts';
import { MPFRError, validate } from '../core.ts';

/**
 * Compare `|b|` and `|c|`: return `+1` if `|b| > |c|`, `0` if `|b| =
 * |c|`, `-1` if `|b| < |c|`. Signs of the operands are ignored.
 *
 * @param b left operand. Must pass {@link validate}.
 * @param c right operand. Must pass {@link validate}.
 * @returns `-1`, `0`, or `+1`.
 * @throws {MPFRError} `EDOMAIN` if either operand has `kind === 'nan'`.
 *   This diverges from the C reference (sets erange + returns 0) per
 *   the documented mpfr-ts NaN policy — see file docstring.
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation (malformed shape).
 *
 * @mpfrName mpfr_cmpabs
 */
export function mpfr_cmpabs(b: MPFR, c: MPFR): number {
  // Structural validation at the trust boundary.
  // Ref: src/ops/cmp.ts — same guard pattern.
  validate(b);
  validate(c);

  // Step 1: NaN. Throw rather than set erange + return 0, matching the
  // mpfr-ts NaN policy established in src/ops/cmp.ts.
  // Ref: mpfr/src/cmpabs.c L39–L42 — MPFR_IS_NAN branch.
  // Ref: CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN".
  if (b.kind === 'nan' || c.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmpabs: NaN operand (b.kind=${b.kind}, c.kind=${c.kind})`,
    );
  }

  // Step 2 & 3: Inf dispatch.
  // Ref: mpfr/src/cmpabs.c L43–L46.
  //   MPFR_IS_INF(b) → return ! MPFR_IS_INF(c)
  //     (1 if c is finite, 0 if c is also Inf)
  //   MPFR_IS_INF(c) → return -1
  if (b.kind === 'inf') {
    // |b| = +∞; |c| is either +∞ (equal) or finite (b wins).
    return c.kind === 'inf' ? 0 : 1;
  }
  if (c.kind === 'inf') {
    // |c| = +∞; b is finite — c wins.
    return -1;
  }

  // Step 4 & 5: Zero dispatch. Neither b nor c is Inf at this point.
  // Ref: mpfr/src/cmpabs.c L47–L50.
  //   MPFR_IS_ZERO(c) → return ! MPFR_IS_ZERO(b)
  //     (1 if b is normal, 0 if b is also zero)
  //   else (b == 0) → return -1
  if (c.kind === 'zero') {
    // |c| = 0; |b| is either 0 (equal) or positive normal (b wins).
    return b.kind === 'zero' ? 0 : 1;
  }
  if (b.kind === 'zero') {
    // |b| = 0; c is normal — c wins.
    return -1;
  }

  // Step 6: Both 'normal'. Compare magnitudes: first by exponent, then
  // by mantissa (MSB-aligned to max prec for cross-prec equality).
  // At this point b.kind === 'normal' && c.kind === 'normal'.
  //
  // Ref: mpfr/src/cmpabs.c L54–L84.

  // Step 6a: Exponent comparison.
  // Ref: mpfr/src/cmpabs.c L54–L57 — be > ce → 1; be < ce → -1.
  if (b.exp > c.exp) return 1;
  if (b.exp < c.exp) return -1;

  // Step 6b: Equal exponents — mantissa comparison with MSB-alignment.
  // The C source walks limbs MSB-first comparing each pair, with two
  // cleanup loops for the shorter operand's remaining limbs (which act
  // as implicit zero words). In BigInt terms: left-shift the
  // lower-precision mantissa so both are aligned to max(b.prec, c.prec)
  // bits; then a single comparison subsumes all three C loops because
  // the left-shift fills in exactly the trailing zeros that C checks.
  //
  // Ref: mpfr/src/cmpabs.c L59–L84 — limb-by-limb MSB-first walk
  //   (same technique as cmp_raw.ts L173–L177 in the signed case).
  // Ref: src/ops/cmp.ts §"Same-value-different-prec" — full rationale.
  const maxPrec: bigint = b.prec > c.prec ? b.prec : c.prec;
  const bAligned: bigint = b.mant << (maxPrec - b.prec);
  const cAligned: bigint = c.mant << (maxPrec - c.prec);

  if (bAligned > cAligned) return 1;
  if (bAligned < cAligned) return -1;
  return 0;
}
