/**
 * ops/div_1.ts — pure-TS port of MPFR's `mpfr_div_1`.
 *
 * Single-limb division fast path: prec(q) == prec(u) == prec(v) = p,
 * where p < GMP_NUMB_BITS (= 64). The C function uses a Möller-Granlund
 * approximate-inverse algorithm to avoid a hardware division; the TS port
 * sidestepping that entirely by using BigInt exact division.
 *
 * C signature:
 *   static int mpfr_div_1(mpfr_ptr q, mpfr_srcptr u, mpfr_srcptr v,
 *                         mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port):
 *   mpfr_div_1(u, v, prec, rnd) -> Result
 *
 * Ref: mpfr/src/div.c L108-L251 — the full prec<64 fast path body.
 * Ref: mpfr/src/div.c L839 — dispatcher routes here when prec < GMP_NUMB_BITS.
 *
 * Algorithm
 * ---------
 *
 * In C, the single-limb mantissas are left-aligned to GMP_NUMB_BITS=64
 * bits (MSB at bit 63). So:
 *   u0 (C) = u.mant << sh     where sh = 64 - prec
 *   v0 (C) = v.mant << sh
 *
 * Step 1 — "extra" flag (Ref: mpfr/src/div.c L120-L121):
 *   extra = (u0 >= v0) ⟺ (u.mant >= v.mant) (since both are shifted by sh).
 *   If extra: u0 -= v0  (i.e., u.mant -= v.mant in prec-bit land).
 *   This normalises the numerator so u0 < v0, i.e. the quotient of
 *   u0 * 2^64 / v0 lies in [0, 2^64). In the result: qx += extra.
 *
 * Step 2 — exact quotient via bigint (replaces Möller-Granlund approx):
 *   We need floor(u0_shifted * 2^64 / v0_shifted), which is equivalent to
 *   floor((u.mant_adj << 64) / v.mant) where u.mant_adj = u.mant - (extra ? v.mant : 0).
 *   Actually: floor(u0 * 2^64 / v0) = floor(u.mant_adj * 2^(64+sh) / (v.mant << sh))
 *           = floor(u.mant_adj * 2^64 / v.mant).
 *   Then q0 = that >> extra (right shift by 0 or 1).
 *
 *   In prec-bit terms:
 *   The floor quotient q0 occupies bits [0, 63] (after >> extra it's a
 *   64-bit value with MSB at bit 63-extra). Then:
 *     rb = q0 & (1 << (sh-1))    round bit: bit sh-1
 *     sb = q0 & ((1<<(sh-1))-1)  sticky bits: bits [0, sh-2]
 *     qp[0] = (HIGHBIT | q0) & ~mask  with mask = (1<<sh)-1
 *           = q0 with the top sh bits cleared and MSB forced on
 *           = (q0 >> sh) << sh | HIGHBIT
 *           = q.mant (in C's 64-bit limb) which maps to (q0 >> sh) in TS prec-bit mant
 *
 *   Cleaner in TS: work with prec-bit mantissas throughout.
 *
 * Step 2 (TS version):
 *   We want the exact quotient (u.mant_adj / v.mant) * 2^prec, rounded to give
 *   prec+1 significant bits (1 round bit + sticky).
 *
 *   Use shift = prec + 1 (one extra bit beyond prec for round bit):
 *     num    = u.mant_adj << (prec + 1)
 *     q64    = num / v.mant          (bigint floor division)
 *     rem    = num % v.mant
 *
 *   q64 has bit-length prec or prec+1 depending on whether u.mant_adj < v.mant
 *   (which is always true after the extra subtraction) — wait, that's not right.
 *
 *   After the extra step: u.mant_adj ∈ [0, v.mant). So u.mant_adj / v.mant < 1.
 *   Therefore q64 = floor(u.mant_adj * 2^(prec+1) / v.mant) ∈ [0, 2^(prec+1)).
 *   This is a (prec+1)-bit value with bit-length at most prec+1.
 *
 *   Now the C code computes q0 (64-bit) then extracts:
 *     q.mant = (q0 | HIGHBIT) & ~mask = (high p bits of q0, forced MSB)
 *   In the TS world: q0 = rb from C (= floor(u0 * 2^64 / v0)), which after >>extra
 *   is q0 = floor(u.mant_adj * 2^64 / v.mant) >> extra.
 *   Since extra is 0 or 1, q0 = floor(u.mant_adj * 2^64 / v.mant) (we used up one
 *   bit of the 2^64 to encode the extra=1 case: qx += extra compensates for the
 *   subtraction of v from u).
 *
 *   Wait, I need to re-read more carefully. Let's re-examine:
 *
 *   In the C code (after the extra check at L120-L121):
 *     u0 may be u0 - v0 (if extra=1)
 *   Then the code computes inv ≈ 2^128/v0 - 2^64 (approx inverse),
 *   rb = floor(u0 * inv / 2^64) ≈ floor(u0 * 2^64 / v0)
 *   q0 = rb >> extra
 *
 *   The exact value of floor(u0 * 2^64 / v0) ∈ [0, 2^64) since u0 < v0.
 *   q0 = floor(u0 * 2^64 / v0) >> extra.
 *
 *   Then:
 *     qp[0] = (HIGHBIT | q0) & ~mask   (p-bit mantissa in a 64-bit field)
 *     rb    = q0 & (1 << (sh-1))       (round bit)
 *     sb    = remainder from the exact division | (q0 & ((1<<(sh-1))-1))
 *
 *   The final C mantissa is: (HIGHBIT | q0) & ~mask
 *     = (0x8000000000000000 | q0) with bits [0, sh-1] zeroed
 *     = top p bits of q0, with the MSB forced on.
 *   In TS, this maps to: resultMant = top p bits of q0 with MSB forced on.
 *
 *   The round bit is bit (sh-1) of q0 = bit (63-p) of q0.
 *   The sticky bits are bits [0, sh-2] of q0 PLUS the remainder.
 *
 *   In TS prec-bit terms (since q0 = floor(u0 * 2^64 / v0) >> extra,
 *   and u0 = u.mant_adj << sh, v0 = v.mant << sh):
 *   q0 = floor((u.mant_adj << sh) * 2^64 / (v.mant << sh)) >> extra
 *      = floor(u.mant_adj * 2^64 / v.mant) >> extra
 *
 *   Let Q = floor(u.mant_adj * 2^64 / v.mant).
 *   If extra=0: q0 = Q.
 *   If extra=1: q0 = Q >> 1.
 *
 *   Q ∈ [0, 2^64) since u.mant_adj < v.mant (after the subtraction in extra=1 case).
 *   For extra=1: q0 ∈ [0, 2^63).
 *   But then qp[0] = (HIGHBIT | q0) → for extra=1, HIGHBIT is just providing the MSB
 *   that the 1-bit shift removed. And qx += extra accounts for the exponent increase.
 *
 *   Let me unify: the true exact quotient (un-shifted) is:
 *     u_orig.mant / v.mant * 2^(u.exp - v.exp - prec) (exact rational)
 *   The MPFR result has mantissa ≈ u_orig.mant / v.mant (scaled to prec bits, MSB set).
 *
 *   Since u_orig.mant ≥ v.mant iff extra=1 (u.mant ≥ v.mant), and after subtraction
 *   u.mant_adj = u.mant_orig - extra * v.mant < v.mant:
 *
 *   The exact quotient m/n in [1/2, 2) where m = u_orig.mant, n = v.mant, both in [2^(p-1), 2^p):
 *   - If m >= n: quotient in [1, 2), resultExp = u.exp - v.exp, resultMant ∈ [2^(p-1), 2^p).
 *   - If m < n:  quotient in [1/2, 1), resultExp = u.exp - v.exp - 1, resultMant ∈ [2^(p-1), 2^p).
 *
 *   This maps to the general div.ts algorithm (divNormalNormal). But since div_1 is
 *   a specialized fast path for equal precisions p < 64, we can use a tighter approach:
 *
 *   Use exactly prec+1 extra bits for round+sticky:
 *     num = u.mant << (prec + 1 + 1)   [= u.mant_orig << (prec + 2) for the two-step]
 *
 *   Actually, let me just map to what the C code does, precisely:
 *   C extracts exactly: result mantissa = top p bits of the quotient, round bit = bit p,
 *   sticky = everything below bit p (which includes the C "sb" bits [0..sh-2] of q0
 *   PLUS the exact remainder from dividing u.mant_adj by v.mant).
 *
 *   In TS with prec-bit mantissas:
 *   The exact quotient floor(u.mant_adj * 2^(2*prec) / v.mant) has (at most) 2*prec bits
 *   (since u.mant_adj < v.mant means quotient < 2^prec, and u.mant_adj * 2^(2*prec) / v.mant
 *   < 2^(2*prec)).
 *
 *   Let fullQ = floor(u.mant_adj * 2^(2*prec) / v.mant)   (2*prec or 2*prec-1 bits)
 *   Let fullRem = u.mant_adj * 2^(2*prec) - fullQ * v.mant
 *
 *   Then the top prec bits of fullQ = the mantissa of the quotient (without the hidden 1 bit),
 *   the next 1 bit = round bit, and the remaining prec-1 bits + fullRem != 0 = sticky.
 *
 *   BUT: we also need to or-in the `extra` bit into the mantissa's leading bit (which is
 *   always set — this is what the `HIGHBIT | q0` does in C).
 *
 *   Actually, let me step back and think cleanly:
 *
 *   The result of u_orig / v is in [1/2, 2) in absolute value (since both are MPFR normals).
 *   Case A (u.mant >= v.mant, extra=1): quotient ∈ [1, 2), resultExp = u.exp - v.exp.
 *   Case B (u.mant < v.mant, extra=0):  quotient ∈ [1/2, 1), resultExp = u.exp - v.exp - 1.
 *
 *   In both cases, the mantissa = quotient / 2^(resultExp - prec) ∈ [2^(p-1), 2^p).
 *   In case A: mantissa = (u.mant / v.mant) scaled to prec bits.
 *   In case B: mantissa = (u.mant / v.mant) * 2 scaled to prec bits (since resultExp - prec is one
 *   less, we multiply by 2).
 *
 *   So in both cases we want prec+1 significant bits of (u.mant / v.mant * scale) where scale=1
 *   for A and scale=2 for B.
 *
 *   More concretely:
 *   Case A: num = u.mant_adj << (prec + 1), divisor = v.mant
 *            q = num / divisor ∈ [0, 2^(prec+1))
 *            top p bits = q >> 1 (mantissa without leading 1)
 *            round bit = q & 1
 *            sticky = rem != 0
 *            But we also need to OR in the leading 1: mant = (q >> 1) | (1n << (prec - 1n))
 *            Wait no: u.mant_adj = u.mant - v.mant in case A. This gives the fractional
 *            part. The integer part (the leading 1) comes from the extra=1 division.
 *            In case A: u.mant / v.mant = 1 + u.mant_adj / v.mant.
 *            So mantissa = (1 + u.mant_adj / v.mant) * 2^(prec-1)  [scaled to have MSB at prec-1]
 *                        = 2^(prec-1) + (u.mant_adj / v.mant) * 2^(prec-1)
 *
 *   Case B: u.mant_adj = u.mant (no subtraction).
 *            u.mant / v.mant ∈ [1/2, 1). After scaling by 2 (resultExp - 1):
 *            mantissa = (u.mant / v.mant) * 2^prec ∈ [2^(prec-1), 2^prec).
 *
 *   Let me use the unified approach from div.ts:
 *   Compute q_full = floor(u.mant << (prec + 1 + v.prec - u.prec)) / v.mant
 *   Since u.prec == v.prec == prec: shift = prec + 1.
 *   q_full = floor(u.mant << (prec + 1)) / v.mant.
 *   Bit-length of q_full: u.mant ∈ [2^(p-1), 2^p), so u.mant << (p+1) ∈ [2^(2p), 2^(2p+1)).
 *   v.mant ∈ [2^(p-1), 2^p), so q_full ∈ [2^p, 2^(2p+1)/2^(p-1)) = [2^p, 2^(p+2)).
 *
 *   So q_full has p+1 or p+2 bits.
 *   - If q_full >= 2^(p+1): it's p+2 bits → resultExp = u.exp - v.exp + 1 (this is case A!
 *     when u.mant >= v.mant: u.mant << (p+1) >= v.mant << (p+1) >= v.mant * 2^p >= 2^(2p-1)*2
 *     so q_full >= 2^p * 2 = 2^(p+1). Actually:
 *     u.mant >= v.mant iff q_full = floor(u.mant * 2^(p+1) / v.mant) >= 2^(p+1).)
 *   - If q_full < 2^(p+1): it's p+1 bits → resultExp = u.exp - v.exp.
 *
 *   That's the general div.ts approach. Let me just use it directly.
 *
 * Ref: mpfr/src/div.c L108-L251.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from '../core.ts';

// Overflow/underflow delegates.
// Ref: src/ops/overflow.ts, src/ops/underflow.ts.
import { mpfr_overflow } from './overflow.ts';
import { mpfr_underflow } from './underflow.ts';

/** Default emax. Ref: mpfr/src/mpfr.h L231. */
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n;
/** Default emin. Ref: mpfr/src/mpfr.h L232. */
const EMIN_DEFAULT: bigint = -((1n << 30n) - 1n);

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ.
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  return false;
}

/**
 * Product-of-signs: sign(u/v) = sign(u) * sign(v).
 * Ref: mpfr/src/div.c L177 — MPFR_MULT_SIGN(MPFR_SIGN(u), MPFR_SIGN(v)).
 */
function multSign(a: Sign, b: Sign): Sign {
  return (a * b) as Sign;
}

function validateArgs(prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Single-limb division fast path for prec(q) = prec(u) = prec(v) = p < 64.
 *
 * Operates on MPFR normal values only. The caller (mpfr_div dispatcher) is
 * responsible for routing special cases (NaN, Inf, zero) elsewhere; this
 * function does not check for them.
 *
 * @mpfrName mpfr_div_1
 *
 * @param u   Dividend — kind must be 'normal', prec < 64.
 * @param v   Divisor  — kind must be 'normal', prec < 64, same prec as u.
 * @param prec Target precision in bits (= u.prec = v.prec), in [1, 63].
 * @param rnd Rounding mode.
 *
 * @returns {value, ternary} where value passes validate() and ternary is
 *          sign(rounded - exact).
 *
 * @throws {MPFRError} EPREC on bad prec; EROUND on bad rounding mode.
 *
 * Ref: mpfr/src/div.c L108-L251 — the C reference body.
 */
export function mpfr_div_1(
  u: MPFR,
  v: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // Ref: mpfr/src/div.c L113 — qx = EXP(u) - EXP(v).
  const qx0: bigint = u.exp - v.exp;

  // Product-of-signs rule.
  // Ref: mpfr/src/div.c L177 — MPFR_MULT_SIGN(MPFR_SIGN(u), MPFR_SIGN(v)).
  const resultSign: Sign = multSign(u.sign, v.sign);

  // --- Compute the exact bigint quotient ---------------------------------
  //
  // We compute the exact quotient via BigInt integer division with enough
  // shift bits to extract the round bit and determine the sticky bit.
  //
  // Strategy (mirrors the `#else` branch of the C code, which uses the
  // clean `udiv_qrnnd` on non-64-bit platforms — the 64-bit path is just
  // an optimised approximation of the same thing):
  //
  //   sh = 64 - p  (number of "tail" bits in a 64-bit limb)
  //   u0 = u.mant << sh  (left-align to 64 bits)
  //   v0 = v.mant << sh
  //   extra = (u0 >= v0) ⟺ (u.mant >= v.mant)
  //   if extra: u0 -= v0  (u.mant_adj = u.mant - v.mant in prec-bit land)
  //   q0, remainder = divmod(u0 * 2^64, v0)  [udiv_qrnnd equivalent]
  //   rb = q0 & (1 << (sh-1))
  //   sb = remainder | (q0 & ((1<<(sh-1))-1))
  //   qp[0] = (HIGHBIT | q0) & ~mask   where mask = (1<<sh)-1
  //         = top p bits of q0 with MSB forced on
  //   qx = qx0 + extra
  //
  // In TS with prec-bit mantissas (not 64-bit aligned), we do exactly
  // the same arithmetic but stay in prec-bit land.
  //
  // Since u0 and v0 differ from u.mant / v.mant only by a common factor
  // of 2^sh, divmod(u0 * 2^64 / v0) = divmod(u.mant_adj * 2^64 / v.mant).
  //
  // And q0 (a 64-bit value) can be factored as:
  //   q0 = (bigint quotient in [0, 2^64)) with top p bits = mant, next 1 = rb, etc.
  //
  // In TS: we compute floor(u.mant_adj * 2^(prec + 1) / v.mant) to get p+1 bits.
  // The top p bits are the mantissa's "tail" (without the leading 1), the
  // last 1 bit is the round bit. The exact division remainder tells us sticky.
  //
  // Ref: mpfr/src/div.c L168-L172 — the #else branch (udiv_qrnnd-based):
  //   udiv_qrnnd(q0, sb, u0, 0, v0)  → q0 = floor(u0 * 2^64 / v0), sb = remainder
  //   sb |= q0 & extra               → if extra, any nonzero in q0 counts as sticky
  //   q0 >>= extra
  //   rb = q0 & (1 << (sh-1))
  //   sb |= q0 & ((1<<(sh-1))-1)

  const uMant: bigint = u.mant;
  const vMant: bigint = v.mant;

  // extra = (u.mant >= v.mant)  [same as u0 >= v0 since sh cancels]
  // Ref: mpfr/src/div.c L120.
  const extra: boolean = uMant >= vMant;
  const uMantAdj: bigint = extra ? uMant - vMant : uMant;

  // q0 = floor(uMantAdj * 2^64 / vMant) where 64 = GMP_NUMB_BITS.
  // In prec-bit terms: floor(uMantAdj * 2^64 / vMant).
  // 2^64 = 2^sh * 2^prec, and in prec-bit world:
  //   floor(uMantAdj * 2^prec / vMant) is a prec-bit value with the result
  //   mantissa in its top bits, but we need one more bit (the round bit)
  //   at position sh-1 (counting from 0 in the 64-bit limb).
  //
  // Equivalently: compute the exact quotient floor(uMantAdj * 2^64 / vMant).
  // After >> extra, this gives q0 (as in C). Then:
  //   - Top p bits of q0 = (q0 >> sh) = the quotient mantissa's "tail" part
  //   - Bit sh-1 of q0 = round bit
  //   - Bits [0, sh-2] of q0 = extra sticky bits in q0
  //   - The exact remainder from the division = sticky from below q0.
  //
  // In TS prec-bit terms: sh = 64 - prec. We want bits [sh, 63] of q0 as mant,
  // bit sh-1 as round, bits [0, sh-2] as q0_sticky.
  //
  // Compute: fullQ = floor(uMantAdj * 2^(prec + 1 + (sh-1))) / vMant
  //               = floor(uMantAdj * 2^64 / vMant) >> extra
  // But since sh = 64 - prec:
  //   prec + 1 + (sh-1) = prec + sh = 64.
  // So: fullQ = floor(uMantAdj << 64 / vMant) = the full q0 before >>extra.
  //
  // Then q0 = fullQ >> extra (0 or 1 right shift).
  //
  // mant portion:   q0 >> (sh - 1) >> 1 = q0 >> sh  ← these are the top p bits
  //   wait: 64-bit q0 has 64 bits. top p bits = bits [63, 64-p] = bits [63, sh].
  //   So: mantTail = q0 >> sh.  (= top p bits, right-justified, in [0, 2^p).)
  //   Full mantissa = mantTail | (1n << (prec - 1n))  ← force MSB on.
  //   Wait: q0 itself doesn't have the leading 1 if we used uMantAdj (which had v subtracted).
  //   C does: qp[0] = (HIGHBIT | q0) & ~mask. HIGHBIT = 1 << 63. So the leading 1 is
  //   always forced on in the 64-bit representation. In prec-bit terms: the leading 1
  //   is the MSB at position prec-1. So: resultMant = (q0 >> sh) | (1n << (prec - 1n)).
  //   But q0 ∈ [0, 2^64) and the top p bits are at positions [63, sh]. After >> sh:
  //   mantTail = q0 >> (64n - prec) ∈ [0, 2^prec). Then OR with HIGHBIT → sets bit 63
  //   in 64-bit land = sets bit prec-1 in prec-bit land.
  //
  // Let's just work with 64-bit arithmetic throughout (all our bigints will be < 2^64).

  const SIXTY_FOUR = 64n;
  const sh = SIXTY_FOUR - prec;  // number of tail/shift bits (GMP_NUMB_BITS - p)

  // fullQ = floor(uMantAdj * 2^64 / vMant)
  // Since uMantAdj < vMant (post-subtraction), fullQ < 2^64.
  // Ref: mpfr/src/div.c L132-L134 (q0 = rb >> extra, where rb ≈ floor(u0*2^64/v0)).
  const fullQ: bigint = (uMantAdj << SIXTY_FOUR) / vMant;
  const fullRem: bigint = (uMantAdj << SIXTY_FOUR) - fullQ * vMant;

  // q0 = fullQ >> extra
  // Ref: mpfr/src/div.c L134.
  let q0: bigint = extra ? fullQ >> 1n : fullQ;
  let sb0: bigint;  // C's "sb" before the refinement — the exact remainder
  if (extra) {
    // When we right-shift fullQ by 1, the dropped LSB (fullQ & 1) contributes
    // to the sticky (as it would have been part of the quotient that's now
    // being squashed into the "remainder" space).
    // In C: sb = remainder from udiv_qrnnd(u0 - v0, 0, v0).
    // Actually: `sb |= q0 & extra` in C (at L169) means before >>extra:
    //   sb |= fullQ & 1  (since extra=1 here).
    // So the sticky bit from the subtraction step is (fullRem != 0) | (fullQ & 1n).
    sb0 = fullRem | (fullQ & 1n);
  } else {
    sb0 = fullRem;
  }

  // Extract the round bit and lower sticky bits from q0.
  // Ref: mpfr/src/div.c L170-L172:
  //   rb = q0 & (MPFR_LIMB_ONE << (sh - 1))
  //   sb |= q0 & ((1<<(sh-1))-1)   [= mask >> 1]
  const rbBit: bigint = (sh >= 1n) ? (1n << (sh - 1n)) : 0n;
  const maskLow: bigint = (sh >= 1n) ? rbBit - 1n : 0n;

  const rb: boolean = (q0 & rbBit) !== 0n;
  const sb: boolean = ((q0 & maskLow) | sb0) !== 0n;

  // The result mantissa in the C 64-bit limb:
  //   qp[0] = (HIGHBIT | q0) & ~mask   where mask = (1<<sh)-1 = maskLow | rbBit
  // In TS prec-bit terms:
  //   The top p bits of q0 (bits [63, sh] in 64-bit) are q0 >> sh.
  //   We force the MSB on: resultMant = (q0 >> sh) | (1n << (prec - 1n)).
  // Ref: mpfr/src/div.c L175.
  const mask = (sh > 0n) ? (rbBit | maskLow) : 0n;  // = (1 << sh) - 1
  const q0Truncated: bigint = q0 & ~mask;  // = q0 with bottom sh bits zeroed
  const resultMant: bigint = ((1n << (SIXTY_FOUR - 1n)) | q0Truncated) >> sh;
  // = (HIGHBIT | q0Truncated) >> sh, where HIGHBIT = 2^63.
  // = (2^63 | q0Truncated) >> (64 - prec)  which gives a prec-bit number with MSB set.

  // Result exponent: qx = (u.exp - v.exp) + extra.
  // Ref: mpfr/src/div.c L113, L176.
  const qx: bigint = qx0 + (extra ? 1n : 0n);

  // Overflow check.
  // Ref: mpfr/src/div.c L180-L181.
  if (qx > EMAX_DEFAULT) {
    return mpfr_overflow(prec, rnd, resultSign);
  }

  // Underflow check (before rounding).
  // Ref: mpfr/src/div.c L186-L204.
  if (qx < EMIN_DEFAULT) {
    // For RNDN, if q is too small (|q| <= 2^(emin-2)), we must use RNDZ.
    // The C code checks: qx < emin-1 OR (qp[0] == HIGHBIT && sb == 0).
    // Ref: mpfr/src/div.c L200-L202.
    let rndForUnderflow = rnd;
    if (rnd === 'RNDN') {
      const mantIsHighbitOnly: boolean = resultMant === (1n << (prec - 1n));
      if (qx < EMIN_DEFAULT - 1n || (mantIsHighbitOnly && !sb)) {
        rndForUnderflow = 'RNDZ';
      }
    }
    return mpfr_underflow(prec, rndForUnderflow, resultSign);
  }

  // --- Rounding ---------------------------------------------------------
  // Ref: mpfr/src/div.c L208-L250.

  if (!rb && !sb) {
    // Exact result.
    return {
      value: {
        kind: 'normal',
        sign: resultSign,
        prec,
        exp: qx,
        mant: resultMant,
      },
      ternary: 0,
    };
  }

  // For RNDN: sb is guaranteed nonzero when rb=0 or rb=1 (see C comment at L215-L219).
  // We cannot be at the midpoint in division of p-bit / p-bit numbers. So:
  //   if rb=0: truncate (= round down in magnitude).
  //   if rb=1: add one ulp.
  // For RNDZ / RNDD (toward zero from positive): truncate.
  // For RNDU / RNDA (away from zero): add one ulp.

  let increment: boolean;
  if (rnd === 'RNDN') {
    // RNDN: rb=0 → truncate, rb=1 → round up.
    // (The midpoint tie-to-even doesn't apply here since sb != 0 always holds.)
    // Ref: mpfr/src/div.c L213-L224.
    increment = rb;
  } else if (isLikeRNDZ(rnd, resultSign)) {
    // RNDZ, or RNDD with positive sign, or RNDU with negative sign.
    // Ref: mpfr/src/div.c L225-L229 — truncate label.
    increment = false;
  } else {
    // Round away from zero: RNDA, RNDU with positive sign, RNDD with negative sign.
    // Ref: mpfr/src/div.c L231-L249 — add_one_ulp label.
    increment = true;
  }

  if (!increment) {
    // Truncate: rounded value is smaller in magnitude than exact.
    // Ternary = sign(rounded - exact). For positive: rounded < exact → ternary = -1.
    // Ref: mpfr/src/div.c L229 — MPFR_RET(-MPFR_SIGN(q)).
    const ternary = (resultSign === 1 ? -1 : 1) as -1 | 0 | 1;
    return {
      value: {
        kind: 'normal',
        sign: resultSign,
        prec,
        exp: qx,
        mant: resultMant,
      },
      ternary,
    };
  }

  // Add one ulp to resultMant.
  // The C comment at L235-L248 proves no carry-overflow can happen.
  // Ref: mpfr/src/div.c L234 — qp[0] += MPFR_LIMB_ONE << sh.
  // In TS prec-bit terms: the ulp is 1 (the LSB of a prec-bit mantissa).
  const ulp = 1n;
  const incremented = resultMant + ulp;
  const upperBound = 1n << prec;
  // The C assertion at L235 confirms qp[0] != 0 after increment, meaning no carry.
  // In prec-bit terms: incremented cannot be 2^prec (= overflow into the next
  // exponent) — the C proof guarantees this.
  if (incremented >= upperBound) {
    // This should not happen per C's proof. Handle defensively by carrying.
    const mant = upperBound >> 1n;
    const ternary = (resultSign === 1 ? 1 : -1) as -1 | 0 | 1;
    return {
      value: {
        kind: 'normal',
        sign: resultSign,
        prec,
        exp: qx + 1n,
        mant,
      },
      ternary,
    };
  }

  // Ternary = sign(rounded - exact). For positive: rounded > exact → +1.
  // Ref: mpfr/src/div.c L249 — MPFR_RET(MPFR_SIGN(q)).
  const ternary = (resultSign === 1 ? 1 : -1) as -1 | 0 | 1;
  return {
    value: {
      kind: 'normal',
      sign: resultSign,
      prec,
      exp: qx,
      mant: incremented,
    },
    ternary,
  };
}
