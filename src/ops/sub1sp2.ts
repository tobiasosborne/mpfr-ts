/**
 * ops/sub1sp2.ts — pure-TS port of MPFR's `mpfr_sub1sp2`.
 *
 * The same-precision, same-sign subtraction fast path for
 * `GMP_NUMB_BITS < p < 2 * GMP_NUMB_BITS`, i.e. 65 <= p <= 127 on x86_64.
 * This is the two-limb case: mantissas occupy [65..127] significant bits.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_sub1sp2(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                            mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_sub1sp2(b, c, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS)
 * ----------------------------------------------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.sign === c.sign` (same-sign; caller ensures this)
 *   - `b.prec === c.prec` and `64n < b.prec < 128n` (i.e. 65..127 bits)
 *
 * Two-limb C layout
 * -----------------
 *
 * C mantissa bp[0..1] with GMP little-endian convention: bp[0] is the
 * least-significant 64-bit limb, bp[1] is the most-significant 64-bit limb.
 * The 128-bit value is: bp[1] * 2^64 + bp[0], MSB-aligned to 128 bits
 * (so bp[1] has its MSB set for normalised values).
 *
 * TS schema stores `mant` MSB-aligned to `p` bits; converting to C form:
 *   climbMant = tsMant << sh   (where sh = 128 - p)
 *   bp1 = climbMant >> 64; bp0 = climbMant & MASK64
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub1sp.c L513-L772 — verbatim C reference body.
 *   - mpfr/src/sub1sp.c L1478-L1479 — dispatcher routing.
 *   - mpfr/src/sub1.c L66-L74 — cancellation-zero sign rule.
 *   - src/ops/underflow.ts — delegate on bx < emin.
 *   - src/ops/sub1sp1n.ts — sister op for p == 64.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Signed zero is real" — RNDD cancellation gives -0.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../core.ts';
import { MPFRError, posZero, negZero } from '../core.ts';
import { mpfr_underflow } from './underflow.ts';

// ---------------------------------------------------------------------------
// Constants
// Ref: mpfr/src/mpfr-impl.h L1300-L1311
// ---------------------------------------------------------------------------

const GMP_NUMB_BITS = 64n;
const MASK64 = (1n << GMP_NUMB_BITS) - 1n;
const LIMB_HIGHBIT = 1n << 63n;

/**
 * Default minimum exponent. Mirrors MPFR_EMIN_DEFAULT.
 * Ref: mpfr/src/mpfr.h L231-L232.
 */
const EMIN_DEFAULT: bigint = -((1n << 30n) - 1n);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Count leading zeros of a 64-bit value (must be nonzero).
 * Mirrors GMP count_leading_zeros macro.
 */
function clz64(x: bigint): bigint {
  if (x === 0n) throw new MPFRError('EPREC', 'clz64: argument is zero');
  let cnt = 0n;
  let v = x;
  if ((v >> 32n) === 0n) { cnt += 32n; v = v << 32n; }
  if ((v >> 48n) === 0n) { cnt += 16n; v = v << 16n; }
  if ((v >> 56n) === 0n) { cnt += 8n; v = v << 8n; }
  if ((v >> 60n) === 0n) { cnt += 4n; v = v << 4n; }
  if ((v >> 62n) === 0n) { cnt += 2n; v = v << 2n; }
  if ((v >> 63n) === 0n) { cnt += 1n; }
  return cnt;
}

/**
 * MPFR_IS_LIKE_RNDZ: does `rnd` round toward zero w.r.t. the sign?
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

/**
 * Build a normal MPFR from a 128-bit C-limb-form mantissa (ap1, ap0)
 * and the resulting exponent/sign. The C form is MSB-aligned to 128 bits;
 * the TS schema stores mant MSB-aligned to p bits (shift right by sh=128-p).
 *
 * TS mant = (ap1 * 2^64 + ap0) >> sh.
 * Ref: mpfr/src/sub1sp.c — C stores ap[1]/ap[0], TS recovers mant via shift.
 */
function buildNormal2(
  p: bigint,
  sh: bigint,
  sign: Sign,
  bx: bigint,
  ap1: bigint,
  ap0: bigint,
): MPFR {
  const full = (ap1 << GMP_NUMB_BITS) | ap0;
  const mant = full >> sh;
  return {
    kind: 'normal',
    sign,
    prec: p,
    exp: bx,
    mant,
  } satisfies MPFR;
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign subtraction fast path for 65 <= p <= 127.
 *
 * @mpfrName mpfr_sub1sp2
 *
 * @param b   Minuend; must be `kind='normal'`, `65 <= prec <= 127`.
 * @param c   Subtrahend; same kind, sign, and prec.
 * @param rnd Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }`.
 *
 * @throws {MPFRError} `EPREC` on bad inputs; `EROUND` on bad rnd.
 */
export function mpfr_sub1sp2(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/sub1sp.c L528 — MPFR_ASSERTD(GMP_NUMB_BITS < p && p < 2 * GMP_NUMB_BITS)
  // -------------------------------------------------------------------------
  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp2: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp2: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_sub1sp2: b.prec (${b.prec}) !== c.prec (${c.prec})`);
  }
  const p = b.prec;
  if (p <= GMP_NUMB_BITS || p >= 2n * GMP_NUMB_BITS) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sub1sp2: prec must be in (64, 128), got ${p}`,
    );
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_sub1sp2: b.sign !== c.sign`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_sub1sp2: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Set up C-mirror variables.
  // sh = 128 - p. This is the number of low bits in the 128-bit C-limb form
  // that are NOT part of the p-bit significand.
  // Ref: mpfr/src/sub1sp.c L597 — sh = 2 * GMP_NUMB_BITS - p.
  // -------------------------------------------------------------------------
  const sh = 2n * GMP_NUMB_BITS - p;  // sh in [1, 63]

  // mask covers the low `sh` bits — round + sticky bit positions in ap0.
  // Ref: mpfr/src/sub1sp.c L598 — mask = MPFR_LIMB_MASK(sh).
  const mask = (1n << sh) - 1n;

  // Convert TS-schema mant (MSB-aligned to p bits) to C 2-limb form
  // (MSB-aligned to 128 bits): climbMant = mant << sh.
  // bp1 = high 64 bits (MSW), bp0 = low 64 bits (LSW).
  const bClimb = b.mant << sh;
  const cClimb = c.mant << sh;
  const bp1 = bClimb >> GMP_NUMB_BITS;
  const bp0 = bClimb & MASK64;
  const cp1 = cClimb >> GMP_NUMB_BITS;
  const cp0 = cClimb & MASK64;

  let bx: bigint = b.exp;
  const cx: bigint = c.exp;

  // Output sign: determined below (initially b.sign, flipped if |c| > |b|).
  let sign: Sign = b.sign;

  let ap1: bigint;
  let ap0: bigint;
  let rb: bigint;
  let sb: bigint;

  if (bx === cx) {
    // -----------------------------------------------------------------------
    // Case: equal exponents — subtraction is exact.
    // Ref: mpfr/src/sub1sp.c L530-L578.
    //
    // a0 = bp[0] - cp[0]
    // a1 = bp[1] - cp[1] - (bp[0] < cp[0])   [borrow from low limb]
    // -----------------------------------------------------------------------
    let a0 = (bp0 - cp0) & MASK64;
    const borrow0 = bp0 < cp0 ? 1n : 0n;
    let a1 = (bp1 - cp1 - borrow0) & MASK64;

    if (a1 === 0n && a0 === 0n) {
      // Exact cancellation → result is zero.
      // Ref: mpfr/src/sub1sp.c L536-L544 and sub1.c L66-L74 (sign rule).
      const zero = rnd === 'RNDD' ? negZero(p) : posZero(p);
      return { value: zero, ternary: 0 };
    }

    // In C (unsigned 64-bit arithmetic): a1 >= bp[1] means a borrow propagated
    // through the high limb, i.e. |c| > |b|. The C check is:
    //   else if (a1 >= bp[1])   [line 545]
    // Ref: mpfr/src/sub1sp.c L545.
    if (a1 >= bp1) {
      // |c| > |b|: output sign is opposite of b.sign.
      sign = (-b.sign) as Sign;
      // Negate the 2-limb value (a = -a mod 2^128).
      // a0 = -a0; a1 = -a1 - (a0 != 0)
      // Ref: mpfr/src/sub1sp.c L549-L550.
      a0 = (-a0) & MASK64;
      a1 = (-a1 - (a0 !== 0n ? 1n : 0n)) & MASK64;
    }
    // else: sign stays b.sign (a1 < bp1 means a1 >= 0 without borrow, so |b| > |c|).

    // Normalise so that a1 has its MSB set.
    // Ref: mpfr/src/sub1sp.c L555-L575.
    if (a1 === 0n) {
      // High limb is zero; shift the value up by GMP_NUMB_BITS.
      // Ref: mpfr/src/sub1sp.c L555-L559.
      a1 = a0;
      a0 = 0n;
      bx -= GMP_NUMB_BITS;
    }
    // Now a1 != 0.
    const cnt = clz64(a1);
    if (cnt > 0n) {
      // Ref: mpfr/src/sub1sp.c L564-L568.
      ap1 = ((a1 << cnt) | (a0 >> (GMP_NUMB_BITS - cnt))) & MASK64;
      ap0 = (a0 << cnt) & MASK64;
      bx -= cnt;
    } else {
      // No leading zeros in a1; already normalised.
      // Ref: mpfr/src/sub1sp.c L570-L572.
      ap1 = a1;
      ap0 = a0;
    }
    // Exact subtraction: no round or sticky bits.
    // Ref: mpfr/src/sub1sp.c L576 — `rb = sb = 0`.
    rb = 0n;
    sb = 0n;
  } else {
    // -----------------------------------------------------------------------
    // Case: different exponents.
    // Ensure bx >= cx by swapping if necessary.
    // Ref: mpfr/src/sub1sp.c L583-L594.
    // -----------------------------------------------------------------------
    let Lbp1: bigint; let Lbp0: bigint;
    let Lcp1: bigint; let Lcp0: bigint;
    if (bx < cx) {
      // Swap b ↔ c: the operand with the larger exponent is now "b".
      // Ref: mpfr/src/sub1sp.c L583-L591 — swap bx/cx and bp/cp; set opposite sign.
      const tmp = bx; bx = cx;
      // bx is now the larger exponent; ignore old cx (we'll compute d below).
      Lbp1 = cp1; Lbp0 = cp0;
      Lcp1 = bp1; Lcp0 = bp0;
      sign = (-b.sign) as Sign;
      void tmp;
    } else {
      // bx > cx: already in order.
      // Ref: mpfr/src/sub1sp.c L592-L594.
      Lbp1 = bp1; Lbp0 = bp0;
      Lcp1 = cp1; Lcp0 = cp0;
      sign = b.sign;
    }
    // d = bx - cx (unsigned; > 0 since we asserted bx != cx).
    // Note: after swap, bx holds the larger exponent.
    // cx still holds the original c.exp (which we didn't modify), but we need
    // the smaller exponent. Let's recompute: the larger is bx (after potential swap),
    // the smaller is the original other exponent.
    // If we swapped: original bx < cx, so smaller = original bx (not yet modified).
    // If we didn't swap: bx > cx, so smaller = cx.
    // This is getting complicated; let's compute d = |b.exp - c.exp| directly.
    const d = b.exp > cx ? b.exp - cx : cx - b.exp;  // d > 0

    if (d < GMP_NUMB_BITS) {
      // -------------------------------------------------------------------
      // Case B1: 0 < d < 64.
      // Ref: mpfr/src/sub1sp.c L599-L663.
      //
      // t = (cp[1] << (64-d)) | (cp[0] >> d)  — aligned low part of c.
      // a0 = bp[0] - t
      // a1 = bp[1] - (cp[1] >> d) - (bp[0] < t)
      // sb = cp[0] << (64-d)  — neglected lower part of c.
      // -------------------------------------------------------------------
      const shift = GMP_NUMB_BITS - d;
      const t = ((Lcp1 << shift) | (Lcp0 >> d)) & MASK64;
      let a0 = (Lbp0 - t) & MASK64;
      let a1 = (Lbp1 - (Lcp1 >> d) - (Lbp0 < t ? 1n : 0n)) & MASK64;
      let lsb = (Lcp0 << shift) & MASK64;  // sb = neglected part of c

      if (lsb !== 0n) {
        // Ref: mpfr/src/sub1sp.c L621-L631.
        // Subtracting the neglected bits (rounded up to the next ulp):
        a1 = (a1 - (a0 === 0n ? 1n : 0n)) & MASK64;
        a0 = (a0 - 1n) & MASK64;
        // MPFR_ASSERTD(a1 > 0 || a0 > 0) — can't be zero here per the C comment.
        lsb = (-lsb) & MASK64;  // sb = 2^64 - sb (positive remainder)
      }

      if (a1 === 0n) {
        // Ref: mpfr/src/sub1sp.c L637-L645.
        // This case implies d=1 and lsb=0.
        a1 = a0;
        a0 = 0n;
        bx -= GMP_NUMB_BITS;
      }

      // a1 != 0; normalise.
      // Ref: mpfr/src/sub1sp.c L647-L657.
      const cnt = clz64(a1);
      if (cnt > 0n) {
        ap1 = ((a1 << cnt) | (a0 >> (GMP_NUMB_BITS - cnt))) & MASK64;
        a0 = ((a0 << cnt) | (lsb >> (GMP_NUMB_BITS - cnt))) & MASK64;
        lsb = (lsb << cnt) & MASK64;
        bx -= cnt;
      } else {
        ap1 = a1;
      }

      // Extract rb and sb from a0.
      // Ref: mpfr/src/sub1sp.c L659-L662.
      // sh > 0 since p < 2*GMP_NUMB_BITS.
      rb = a0 & (1n << (sh - 1n));       // round bit = bit (sh-1) of a0
      sb = ((a0 & mask) ^ rb) | lsb;    // sticky = remaining low bits of a0, plus lsb
      ap0 = a0 & ~mask;                  // clear round+sticky bits from stored limb
    } else if (d < 2n * GMP_NUMB_BITS) {
      // -------------------------------------------------------------------
      // Case B2: 64 <= d < 128.
      // Ref: mpfr/src/sub1sp.c L664-L689.
      //
      // sb gets the bits of c that fall below b's LSB position.
      // -------------------------------------------------------------------
      let lsb: bigint;
      if (d === GMP_NUMB_BITS) {
        // d == 64: the entire low limb of c contributes to sticky.
        // Ref: mpfr/src/sub1sp.c L668.
        lsb = Lcp0;
      } else {
        // d > 64: upper bits of cp[1] shifted out contribute; cp[0] is all sticky.
        // Ref: mpfr/src/sub1sp.c L669.
        lsb = ((Lcp1 << (2n * GMP_NUMB_BITS - d)) & MASK64) | (Lcp0 !== 0n ? 1n : 0n);
      }
      // t = (cp[1] >> (d - 64)) + (sb != 0)
      // Ref: mpfr/src/sub1sp.c L670. Note: t may overflow to 0 if d=64 and sb!=0.
      const t = ((Lcp1 >> (d - GMP_NUMB_BITS)) + (lsb !== 0n ? 1n : 0n)) & MASK64;
      const a0 = (Lbp0 - t) & MASK64;
      // a1 = bp[1] - (bp[0] < t) - (t == 0 && sb != 0)
      // Ref: mpfr/src/sub1sp.c L673.
      const borrow1 = Lbp0 < t ? 1n : 0n;
      const borrow2 = t === 0n && lsb !== 0n ? 1n : 0n;
      const a1 = (Lbp1 - borrow1 - borrow2) & MASK64;
      // sb = -sb (negation, as the "other side" of the subtraction).
      // Ref: mpfr/src/sub1sp.c L674.
      let lsb2 = (-lsb) & MASK64;

      // Since bp[1] has MSB set, exponent decrease at most 1.
      // Ref: mpfr/src/sub1sp.c L677.
      if (a1 < LIMB_HIGHBIT) {
        // Shift up by 1.
        // Ref: mpfr/src/sub1sp.c L679-L683.
        ap1 = ((a1 << 1n) | (a0 >> (GMP_NUMB_BITS - 1n))) & MASK64;
        const a0s = ((a0 << 1n) | (lsb2 >> (GMP_NUMB_BITS - 1n))) & MASK64;
        lsb2 = (lsb2 << 1n) & MASK64;
        bx -= 1n;
        rb = a0s & (1n << (sh - 1n));
        sb = ((a0s & mask) ^ rb) | lsb2;
        ap0 = a0s & ~mask;
      } else {
        ap1 = a1;
        rb = a0 & (1n << (sh - 1n));
        sb = ((a0 & mask) ^ rb) | lsb2;
        ap0 = a0 & ~mask;
      }
    } else {
      // -------------------------------------------------------------------
      // Case B3: d >= 128.
      // Ref: mpfr/src/sub1sp.c L690-L721.
      //
      // b - ulp(b): subtract the least significant bit of b (position sh in ap0).
      // The remainder (ulp(b) - c) satisfies 1/2*ulp(b) < ulp(b) - c < ulp(b),
      // so rb = sb = 1 (unless there was an exponent decrease from bp1 == HIGHBIT).
      // -------------------------------------------------------------------
      const t = 1n << sh;  // MPFR_LIMB_ONE << sh
      const a0 = (Lbp0 - t) & MASK64;
      const borrow = Lbp0 < t ? 1n : 0n;
      const a1 = (Lbp1 - borrow) & MASK64;

      if (a1 < LIMB_HIGHBIT) {
        // bp[1] had exactly its MSB set (bp = 1000...000 in high limb).
        // Exponent decreases by 1.
        // Ref: mpfr/src/sub1sp.c L698-L713.
        //
        // rb = (sh > 1 || d > 2*GMP_NUMB_BITS || (cp1 == HIGHBIT && cp0 == 0))
        // Ref: mpfr/src/sub1sp.c L705-L706.
        rb = (sh > 1n || d > 2n * GMP_NUMB_BITS ||
          (Lcp1 === LIMB_HIGHBIT && Lcp0 === 0n))
          ? 1n : 0n;
        // sb = 1 (almost always; the C comment says it may be wrong for one
        // specific corner case but the even rule still rounds up).
        // Ref: mpfr/src/sub1sp.c L707-L709.
        sb = 1n;
        // ap[0] = ~mask (all significant bits set, padding bits 0).
        // ap[1] = MPFR_LIMB_MAX (all 64 bits set).
        // Ref: mpfr/src/sub1sp.c L710-L711.
        ap0 = MASK64 ^ mask;  // ~mask in 64-bit: all bits except low sh set
        ap1 = MASK64;         // MPFR_LIMB_MAX = 2^64 - 1
        bx -= 1n;
      } else {
        // bp[1] has its MSB set and other bits: no exponent decrease.
        // Ref: mpfr/src/sub1sp.c L714-L719.
        ap0 = a0;
        ap1 = a1;
        rb = 1n;
        sb = 1n;
      }
    }
  }

  // -------------------------------------------------------------------------
  // Underflow check.
  // Ref: mpfr/src/sub1sp.c L731-L740.
  //
  // Warning: MPFR considers underflow *after* rounding with unbounded exponent.
  // Since b and c have the same precision p, they're multiples of 2^(emin-p),
  // so bx is also the exponent after rounding with unbounded exponent range.
  // -------------------------------------------------------------------------
  if (bx < EMIN_DEFAULT) {
    let effRnd: RoundingMode = rnd;
    // For RNDN: if |a| <= 2^(emin-2), change to RNDZ to avoid round-away.
    // Condition: bx < emin - 1 OR (ap[1] == LIMB_HIGHBIT && ap[0] == 0).
    // Ref: mpfr/src/sub1sp.c L734-L738.
    if (rnd === 'RNDN' &&
        (bx < EMIN_DEFAULT - 1n || (ap1 === LIMB_HIGHBIT && ap0 === 0n))) {
      effRnd = 'RNDZ';
    }
    return mpfr_underflow(p, effRnd, sign);
  }

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: mpfr/src/sub1sp.c L742-L772.
  //
  // sh > 0 since p < 2*GMP_NUMB_BITS.
  // Ternary: sign of (rounded - exact).
  // -------------------------------------------------------------------------
  if (rb === 0n && sb === 0n) {
    // Exact.
    // Ref: mpfr/src/sub1sp.c L743-L744.
    const value = buildNormal2(p, sh, sign, bx, ap1, ap0);
    return { value, ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // Ref: mpfr/src/sub1sp.c L745-L750.
    // Truncate if rb == 0, OR (sb == 0 AND least-significant bit of result = 0).
    // "bit at position sh of ap[0]" = ap0 & (MPFR_LIMB_ONE << sh).
    const lsbBit = ap0 & (1n << sh);
    if (rb === 0n || (sb === 0n && lsbBit === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    // Ref: mpfr/src/sub1sp.c L752-L754.
    doAddOneUlp = false;
  } else {
    // Round away from zero (RNDA, or RNDU when positive, or RNDD when negative).
    // Ref: mpfr/src/sub1sp.c L757-L771.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // Truncate: ternary = -sign (rounded < exact for positive; rounded > exact for negative).
    // Ref: mpfr/src/sub1sp.c L754 — MPFR_RET(-MPFR_SIGN(a)).
    const value = buildNormal2(p, sh, sign, bx, ap1, ap0);
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value, ternary };
  }

  // add_one_ulp: ap[0] += MPFR_LIMB_ONE << sh; ap[1] += (ap[0] == 0).
  // Ref: mpfr/src/sub1sp.c L760-L769.
  const newAp0 = (ap0 + (1n << sh)) & MASK64;
  let newAp1 = (ap1 + (newAp0 === 0n ? 1n : 0n)) & MASK64;
  let newBx = bx;

  if (newAp1 === 0n) {
    // Mantissa overflow: ap[1] wrapped to 0. Set to HIGHBIT and bump exponent.
    // Ref: mpfr/src/sub1sp.c L762-L767.
    // Note: bx+1 cannot exceed emax since |a| <= |b| (comment at L767).
    newAp1 = LIMB_HIGHBIT;
    newBx = bx + 1n;
  }

  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  const value = buildNormal2(p, sh, sign, newBx, newAp1, newAp0);
  return { value, ternary };
}
