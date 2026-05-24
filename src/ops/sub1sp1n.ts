/**
 * ops/sub1sp1n.ts — pure-TS port of MPFR's `mpfr_sub1sp1n`.
 *
 * Same-precision same-sign subtraction fast path for p == GMP_NUMB_BITS == 64,
 * i.e. exactly one limb with sh == 0 (no sub-limb padding). This is the
 * 64-bit specialization of `mpfr_sub1sp` for equal precisions on both operands.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_sub1sp1n(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                             mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_sub1sp1n(b, c, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS)
 * ----------------------------------------------------
 *
 *   - b.kind === 'normal' and c.kind === 'normal'
 *   - b.sign === c.sign (same-sign subtraction; caller ensures this)
 *   - b.prec === c.prec === 64n
 *
 * Since p == 64 and sh == 0, the TS-schema mantissa (MSB-aligned to prec=64
 * bits) is identical to the C 64-bit limb. No conversion needed.
 *
 * Algorithm overview
 * ------------------
 *
 * Case A (bx == cx, equal exponents): exact subtraction. b0 - c0.
 *   - If zero: return ±0 per rounding mode sign rule.
 *   - If borrow (c > b in magnitude): flip sign, negate result.
 *   - Count leading zeros, left-shift, decrement bx. rb = sb = 0.
 *
 * Case B (bx != cx): align c right by d = bx - cx bits (swap if needed).
 *   B1 (d < 64): sb = -(cp0 << (64 - d)) [two's-complement negation of the
 *     neglected part]. a0 = bp0 - (sb != 0) - (cp0 >> d).
 *     Sub-case a0 == 0: bx -= 64, ap0 = HIGHBIT, rb = sb = 0.
 *     Sub-case a0 != 0: count_leading_zeros + left-shift + extract rb/sb.
 *   B2 (d >= 64): b - ulp(b) computation; rb/sb depend on d and cp0.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub1sp.c L325-L510 — the verbatim C reference body.
 *   - mpfr/src/sub1sp.c L1481-L1484 — dispatcher routes p==GMP_NUMB_BITS here.
 *   - mpfr/src/sub1.c L66-L74 — cancellation-zero sign rule.
 *   - mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 *   - src/ops/underflow.ts — delegate on bx < emin.
 *   - src/ops/add1sp1.ts — sister op (add direction); used as style reference.
 *   - src/core.ts — locked schema (MPFR, RoundingMode, Result, Ternary, Sign).
 *   - CLAUDE.md "Catastrophic cancellation" — post-cancel bit-length recompute.
 *   - CLAUDE.md "Signed zero is real" — RNDD-cancellation gives -0.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import { MPFRError, negZero, posZero } from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import { mpfr_underflow } from '/home/tobias/Projects/mpfr-ts/src/ops/underflow.ts';

// ---------------------------------------------------------------------------
// Constants
// Ref: mpfr/src/mpfr-impl.h L1300-L1311
// ---------------------------------------------------------------------------

/** GMP_NUMB_BITS: limb width on x86_64. For p==64, sh==0. */
const GMP_NUMB_BITS = 64n;

/** All 64 bits set: used for two's-complement arithmetic in 64-bit space. */
const LIMB_MASK_64 = (1n << GMP_NUMB_BITS) - 1n;

/**
 * MPFR_LIMB_HIGHBIT = 1 << 63.
 * Ref: mpfr/src/mpfr-impl.h L1301.
 */
const LIMB_HIGHBIT = 1n << 63n;

/**
 * Default minimum exponent. Mirrors __gmpfr_emin on fresh init.
 * Ref: mpfr/src/mpfr.h L231-L232 — MPFR_EMIN_DEFAULT = -(2^30 - 1).
 */
const EMIN_DEFAULT: bigint = -((1n << 30n) - 1n);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ(rnd_mode, neg):
 *   rounds toward zero w.r.t. the magnitude direction implied by `sign`.
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

/**
 * count_leading_zeros for a 64-bit value.
 * Returns the number of leading zero bits in `v` (where v is in [1, 2^64)).
 * Returns 0 for v == LIMB_HIGHBIT (bit 63 set).
 * Ref: GMP / MPFR count_leading_zeros macro.
 */
function clz64(v: bigint): bigint {
  if (v === 0n) return 64n; // should not be called with 0
  let cnt = 0n;
  let bit = 63n;
  while (bit >= 0n) {
    if ((v >> bit) & 1n) break;
    cnt++;
    bit--;
  }
  return cnt;
}

/**
 * Build an MPFR normal value for p==64 (sh==0: mant == ap0 directly).
 * Ref: src/core.ts — MPFR value shape.
 */
function buildNormal(sign: Sign, exp: bigint, mant: bigint): MPFR {
  return {
    kind: 'normal',
    sign,
    prec: 64n,
    exp,
    mant,
  } satisfies MPFR;
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign subtraction fast path for p == GMP_NUMB_BITS == 64.
 *
 * Mirrors `mpfr_sub1sp1n` from `mpfr/src/sub1sp.c L325-L510`.
 *
 * @mpfrName mpfr_sub1sp1n
 *
 * @param b   Minuend; must be kind='normal', prec=64n.
 * @param c   Subtrahend; must be kind='normal', prec=64n, same sign as b.
 * @param rnd Rounding mode (five modes per src/core.ts).
 *
 * @returns `{ value, ternary }` — the correctly-rounded difference and the
 *          ternary flag (sign of rounded − exact).
 *
 * @throws {MPFRError} EPREC for invalid kind/prec; EROUND for unknown rnd.
 */
export function mpfr_sub1sp1n(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/sub1sp.c L339-L341 — MPFR_ASSERTD checks.
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp1n: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp1n: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== 64n) {
    throw new MPFRError('EPREC', `mpfr_sub1sp1n: b.prec must be 64n, got ${b.prec}`);
  }
  if (c.prec !== 64n) {
    throw new MPFRError('EPREC', `mpfr_sub1sp1n: c.prec must be 64n, got ${c.prec}`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_sub1sp1n: b.sign (${b.sign}) !== c.sign (${c.sign})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_sub1sp1n: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Setup: for p==64, sh==0, so mant is directly the 64-bit limb.
  // Ref: mpfr/src/sub1sp.c L328-L333.
  // -------------------------------------------------------------------------

  // C uses bp[0], cp[0] — for p==64, sh==0, mant == the 64-bit limb.
  let bp0: bigint = b.mant;  // b.mant << 0 = b.mant
  let cp0: bigint = c.mant;
  let bx: bigint = b.exp;
  const cx0: bigint = c.exp;
  let cx: bigint = cx0;

  // The output sign starts as b.sign; may be flipped on borrow or swap.
  // We track it as 'resultSign' and update as the C sets MPFR_SET_OPPOSITE_SIGN.
  let resultSign: Sign = b.sign;

  let ap0: bigint;  // result limb
  let rb: bigint;   // round bit (nonzero <=> set)
  let sb: bigint;   // sticky bit (nonzero <=> any lost bit was set)

  // -------------------------------------------------------------------------
  // Case A: bx == cx (equal exponents, exact subtraction)
  // Ref: mpfr/src/sub1sp.c L343-L369.
  // -------------------------------------------------------------------------

  if (bx === cx) {
    // a0 = bp[0] - cp[0]; both are 64-bit values so this is exact subtraction.
    // Ref: sub1sp.c L345.
    let a0 = bp0 - cp0;  // BigInt subtraction — exact, may be negative

    if (a0 === 0n) {
      // Complete cancellation: return ±0.
      // Sign rule: RNDD → -0, all others → +0.
      // Ref: mpfr/src/sub1sp.c L347-L354 and mpfr/src/sub1.c L66-L74.
      if (rnd === 'RNDD') {
        return { value: negZero(64n), ternary: 0 };
      } else {
        return { value: posZero(64n), ternary: 0 };
      }
    }

    if (a0 < 0n) {
      // Borrow: |c| > |b|, so the result has the opposite sign of b.
      // Ref: sub1sp.c L355-L359 — `if (a0 > bp[0])` (unsigned overflow check).
      // In BigInt arithmetic, borrow is simply a0 < 0.
      resultSign = (b.sign === 1 ? -1 : 1) as Sign;
      a0 = -a0;  // negate to get positive magnitude
    } else {
      // bp[0] > cp[0]: result has same sign as b.
      // Ref: sub1sp.c L360-L361.
      resultSign = b.sign;
    }

    // a0 != 0: count leading zeros, shift left, decrement bx.
    // Ref: sub1sp.c L364-L368.
    const cnt = clz64(a0);
    ap0 = (a0 << cnt) & LIMB_MASK_64;  // left-shift and mask to 64 bits
    bx = bx - cnt;
    rb = 0n;
    sb = 0n;

  } else {
    // -----------------------------------------------------------------------
    // Case B: bx != cx
    // Ref: sub1sp.c L370-L455.
    // -----------------------------------------------------------------------

    // Swap b and c if bx < cx so that bx > cx.
    // The result sign flips when we swap (since the operand that was c becomes
    // the dominant one, which has c's sign but in context c had opposite role).
    // Ref: sub1sp.c L372-L383.
    if (bx < cx) {
      const tx = bx; bx = cx; cx = tx;
      const tp = bp0; bp0 = cp0; cp0 = tp;
      // MPFR_SET_OPPOSITE_SIGN(a, b): result sign flips.
      resultSign = (b.sign === 1 ? -1 : 1) as Sign;
    } else {
      // MPFR_SET_SAME_SIGN(a, b).
      resultSign = b.sign;
    }

    const d: bigint = bx - cx;  // d > 0, unsigned difference

    if (d < GMP_NUMB_BITS) {
      // ---------------------------------------------------------------------
      // Case B1: 0 < d < 64
      // Ref: sub1sp.c L386-L409.
      //
      // sb = -(cp[0] << (GMP_NUMB_BITS - d))  [two's-complement negation]
      //   This is the "neglected part of -c" — the bits of c that fell below
      //   the precision window when c was right-shifted by d.
      //
      // a0 = bp[0] - (sb != 0) - (cp[0] >> d)
      //   The (sb != 0) term borrows 1 from the main word for the fractional part.
      // ---------------------------------------------------------------------

      // Two's-complement negation of the shifted-out bits:
      // sb = -(cp0 << (64 - d)) mod 2^64
      // Ref: sub1sp.c L388 — `sb = - (cp[0] << (GMP_NUMB_BITS - d));`
      const shift = GMP_NUMB_BITS - d;
      const shifted_cp = (cp0 << shift) & LIMB_MASK_64;
      // Two's-complement negation: -x mod 2^64 = (2^64 - x) mod 2^64
      sb = shifted_cp === 0n ? 0n : ((1n << GMP_NUMB_BITS) - shifted_cp) & LIMB_MASK_64;

      // a0 = bp[0] - (sb != 0) - (cp[0] >> d)
      // Ref: sub1sp.c L389.
      const borrow = sb !== 0n ? 1n : 0n;
      const cp_shifted = cp0 >> d;
      let a0 = bp0 - borrow - cp_shifted;

      // Result may underflow to negative in BigInt; handle 64-bit wrap:
      // But since bx > cx and bp0 has MSB set (normal), a0 should be >= 0.
      // However, a0 can be 0 (special case).
      // Mask to 64 bits in case arithmetic wrapped:
      a0 = a0 & LIMB_MASK_64;

      if (a0 === 0n) {
        // Special case: a0 == 0 means result is 2^(bx - GMP_NUMB_BITS) * HIGHBIT.
        // Ref: sub1sp.c L392-L396.
        // Only possible when d=1, bp0=HIGHBIT, cp0=LIMB_MASK_64 (per C comment).
        bx = bx - GMP_NUMB_BITS;
        ap0 = LIMB_HIGHBIT;
        rb = 0n;
        sb = 0n;
      } else {
        // General case: normalize a0.
        // Ref: sub1sp.c L399-L408.
        const cnt = clz64(a0);
        if (cnt > 0n) {
          // a0 = (a0 << cnt) | (sb >> (GMP_NUMB_BITS - cnt))
          // Ref: sub1sp.c L401-L402.
          a0 = ((a0 << cnt) | (sb >> (GMP_NUMB_BITS - cnt))) & LIMB_MASK_64;
          sb = (sb << cnt) & LIMB_MASK_64;
        }
        bx = bx - cnt;
        // rb = sb & MPFR_LIMB_HIGHBIT (the top bit of the fractional part)
        // Ref: sub1sp.c L405.
        rb = sb & LIMB_HIGHBIT;
        // sb = sb & ~MPFR_LIMB_HIGHBIT (remaining sticky bits)
        // Ref: sub1sp.c L406.
        sb = sb & ~LIMB_HIGHBIT & LIMB_MASK_64;
        ap0 = a0;
      }

    } else {
      // ---------------------------------------------------------------------
      // Case B2: d >= 64
      // Ref: sub1sp.c L410-L455 — "We compute b - ulp(b)"
      //
      // When d >= 64, c's entire significand is below the precision window
      // of b. The result is b minus one ulp of b, with rb/sb tracking the
      // lost portion of c.
      // ---------------------------------------------------------------------

      if (bp0 > LIMB_HIGHBIT) {
        // bp[0] > MPFR_LIMB_HIGHBIT: the result is bp[0] - 1.
        // Ref: sub1sp.c L413-L421.
        //
        // If d == GMP_NUMB_BITS:
        //   rb = 0, sb = 1, unless cp[0] == HIGHBIT then rb = 1, sb = 0.
        // If d > GMP_NUMB_BITS:
        //   rb = sb = 1.
        // Combined: rb = (d > GMP_NUMB_BITS || cp0 == HIGHBIT) ? 1 : 0
        //           sb = (d > GMP_NUMB_BITS || cp0 != HIGHBIT) ? 1 : 0
        // Ref: sub1sp.c L418-L419.
        rb = (d > GMP_NUMB_BITS || cp0 === LIMB_HIGHBIT) ? 1n : 0n;
        sb = (d > GMP_NUMB_BITS || cp0 !== LIMB_HIGHBIT) ? 1n : 0n;
        ap0 = bp0 - 1n;  // bp[0] - MPFR_LIMB_ONE
      } else {
        // bp[0] == MPFR_LIMB_HIGHBIT: this is the "shifted by one" case.
        // Ref: sub1sp.c L423-L454.
        //
        // bx-- (the result exponent decreases by 1 since we "shifted right").
        // Ref: sub1sp.c L435.
        bx = bx - 1n;

        if (d === GMP_NUMB_BITS && cp0 > LIMB_HIGHBIT) {
          // Case (b): d=GMP_NUMB_BITS and cp[0] > HIGHBIT.
          // Ref: sub1sp.c L436-L440.
          //
          // rb = MPFR_LIMB(-cp[0]) >= (MPFR_LIMB_HIGHBIT >> 1)
          //    = (-cp0 & MASK64) >= (HIGHBIT >> 1)
          // sb = MPFR_LIMB(-cp[0]) << 2  [nonzero check; just set to the value]
          // ap[0] = -(MPFR_LIMB_ONE << 1) = -2 mod 2^64 = 2^64 - 2
          const neg_cp0 = (-cp0) & LIMB_MASK_64;  // two's complement negation
          rb = neg_cp0 >= (LIMB_HIGHBIT >> 1n) ? 1n : 0n;
          // sb = MPFR_LIMB(-cp[0]) << 2 — this is the sticky: nonzero if << 2 is nonzero
          // Ref: sub1sp.c L439: `sb = MPFR_LIMB(-cp[0]) << 2;`
          // We need the actual value (nonzero check used later).
          sb = (neg_cp0 << 2n) & LIMB_MASK_64;
          ap0 = (-(1n << 1n)) & LIMB_MASK_64;  // -2 mod 2^64 = 0xFFFFFFFFFFFFFFFE
        } else {
          // Cases (a), (c), (d), (e):
          // (a) d=GMP_NUMB_BITS, cp0 == HIGHBIT: a0=111...111, rb=sb=0
          // (c) d=GMP_NUMB_BITS+1, cp0 == HIGHBIT: a0=111...111, rb=1, sb=0
          // (d) d=GMP_NUMB_BITS+1, cp0 > HIGHBIT: a0=111...111, rb=0, sb=1
          // (e) d > GMP_NUMB_BITS+1: a0=111...111, rb=sb=1
          // Ref: sub1sp.c L442-L453.

          // rb=1 in case (e) and case (c)
          // Ref: sub1sp.c L445-L446.
          rb = (d > GMP_NUMB_BITS + 1n || (d === GMP_NUMB_BITS + 1n && cp0 === LIMB_HIGHBIT))
            ? 1n : 0n;
          // sb=1 in case (d) and (e)
          // Ref: sub1sp.c L448-L449.
          sb = (d > GMP_NUMB_BITS + 1n || (d === GMP_NUMB_BITS + 1n && cp0 > LIMB_HIGHBIT))
            ? 1n : 0n;
          // ap[0] = -MPFR_LIMB_ONE = 2^64 - 1 = all ones
          // Ref: sub1sp.c L452.
          ap0 = LIMB_MASK_64;  // 0xFFFFFFFFFFFFFFFF
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Underflow check (post-cancel exponent may be below emin).
  // Ref: sub1sp.c L465-L478.
  //
  // Note from C comments: since b and c have same precision p, b-c is a
  // multiple of 2^(emin-p), so if bx < emin the subtraction is exact and
  // rb = sb = 0. For RNDN, if |a| <= 2^(emin-2) we must use RNDZ instead.
  // -------------------------------------------------------------------------

  if (bx < EMIN_DEFAULT) {
    let effectiveRnd = rnd;
    if (rnd === 'RNDN' &&
        (bx < EMIN_DEFAULT - 1n || ap0 === LIMB_HIGHBIT)) {
      // rb == 0 && sb == 0 (MPFR_ASSERTD in C)
      effectiveRnd = 'RNDZ';
    }
    return mpfr_underflow(64n, effectiveRnd, resultSign);
  }

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: sub1sp.c L481-L509.
  //
  // Ternary convention: sign of (rounded - exact).
  //   truncate  → ternary = -resultSign
  //   add_one_ulp → ternary = +resultSign
  // -------------------------------------------------------------------------

  if (rb === 0n && sb === 0n) {
    // Exact result.
    // Ref: sub1sp.c L482-L483.
    return { value: buildNormal(resultSign, bx, ap0), ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // Round to nearest, ties to even.
    // Ref: sub1sp.c L484-L489.
    //
    // Truncate if:
    //   rb == 0  (below midpoint), OR
    //   rb != 0 AND sb == 0 AND LSB of ap0 is 0 (tie → even)
    //
    // For p==64, sh==0: LSB is bit 0 of ap0, i.e. ap0 & 1.
    // Ref: sub1sp.c L486 — `(ap[0] & MPFR_LIMB_ONE) == 0`
    const lsb = ap0 & 1n;
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, resultSign)) {
    // Truncate toward zero.
    // Ref: sub1sp.c L491-L494.
    doAddOneUlp = false;
  } else {
    // Round away from zero.
    // Ref: sub1sp.c L496-L509.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // Truncate: ternary = -resultSign
    // Ref: sub1sp.c L494 — `MPFR_RET(-MPFR_SIGN(a))`.
    const ternary: Ternary = (resultSign === 1 ? -1 : 1) as Ternary;
    return { value: buildNormal(resultSign, bx, ap0), ternary };
  }

  // add_one_ulp
  // Ref: sub1sp.c L499-L508.
  //
  // ap[0] += MPFR_LIMB_ONE
  // If ap[0] == 0 (overflow): ap[0] = HIGHBIT, bx++.
  // Note from C: bx+1 cannot exceed emax since |a| <= |b|.
  let newAp0 = (ap0 + 1n) & LIMB_MASK_64;
  let newBx = bx;

  if (newAp0 === 0n) {
    // Mantissa overflow: ap[0] = HIGHBIT, increment exponent.
    // Ref: sub1sp.c L500-L506.
    newAp0 = LIMB_HIGHBIT;
    newBx = bx + 1n;
    // Note: C asserts bx+1 <= emax; we trust this since |a| <= |b|.
  }

  const ternary: Ternary = (resultSign === 1 ? 1 : -1) as Ternary;
  return { value: buildNormal(resultSign, newBx, newAp0), ternary };
}
