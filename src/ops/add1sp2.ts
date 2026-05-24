/**
 * ops/add1sp2.ts — pure-TS port of MPFR's `mpfr_add1sp2`.
 *
 * The same-precision, same-sign addition fast path for
 * `GMP_NUMB_BITS < p < 2 * GMP_NUMB_BITS`, i.e. 65 <= p <= 127 on x86_64.
 * This is the two-limb case: mantissas occupy [65..127] significant bits.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_add1sp2(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                            mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_add1sp2(b, c, rnd) -> Result
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
 * The 128-bit value is: bp[1] * 2^64 + bp[0], MSB-aligned to 128 bits.
 * TS schema stores `mant` MSB-aligned to `p` bits; converting:
 *   climbMant = tsMant << sh   (where sh = 128 - p, sh in [1,63])
 *   bp1 = climbMant >> 64; bp0 = climbMant & MASK64
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add1sp.c L360-L480 — verbatim C reference body.
 *   - mpfr/src/add1sp.c L1485-L1486 — dispatcher routing.
 *   - src/ops/sub1sp2.ts — sister op (sub direction) at same prec range.
 *   - src/ops/overflow.ts — delegate.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Ternary flag is the sign of (rounded - exact), not 0/1."
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../src/core.ts';
import { MPFRError } from '../../src/core.ts';
import { mpfr_overflow } from '../../src/ops/overflow.ts';

// ---------------------------------------------------------------------------
// Constants
// Ref: mpfr/src/mpfr-impl.h L1300-L1311
// ---------------------------------------------------------------------------

/** 64-bit limb width: GMP_NUMB_BITS on x86_64. */
const GMP_NUMB_BITS = 64n;

/** 64-bit mask: all 64 bits set. */
const MASK64 = (1n << GMP_NUMB_BITS) - 1n;

/** MPFR_LIMB_HIGHBIT = 1 << 63. Ref: mpfr/src/mpfr-impl.h L1301. */
const LIMB_HIGHBIT = 1n << 63n;

/**
 * Default exponent ceiling — matches `__gmpfr_emax` on fresh init.
 * Ref: mpfr/src/mpfr.h L231 — `MPFR_EMAX_DEFAULT = (1 << 30) - 1`.
 */
const EMAX_DEFAULT = (1n << 30n) - 1n;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ (mpfr-impl.h L1233-L1234):
 * "does `rnd` round toward zero with respect to the sign?"
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
 * Ref: mpfr/src/add1sp.c — C stores ap[1]/ap[0], TS recovers mant via shift.
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
 * Same-precision same-sign addition fast path for 65 <= p <= 127.
 *
 * @mpfrName mpfr_add1sp2
 *
 * @param b   First addend; must be `kind='normal'`, `65 <= prec <= 127`.
 * @param c   Second addend; must be `kind='normal'`, same `prec` and `sign`.
 * @param rnd Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }`.
 *
 * @throws {MPFRError} `EPREC` on bad inputs; `EROUND` on bad rnd.
 */
export function mpfr_add1sp2(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/add1sp.c L377 — MPFR_ASSERTD(GMP_NUMB_BITS < p && p < 2 * GMP_NUMB_BITS)
  // -------------------------------------------------------------------------
  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp2: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp2: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_add1sp2: b.prec (${b.prec}) !== c.prec (${c.prec})`);
  }
  const p = b.prec;
  if (p <= GMP_NUMB_BITS || p >= 2n * GMP_NUMB_BITS) {
    throw new MPFRError(
      'EPREC',
      `mpfr_add1sp2: prec must be in (64, 128), got ${p}`,
    );
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_add1sp2: b.sign !== c.sign (${b.sign} vs ${c.sign})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_add1sp2: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Set up C-mirror variables.
  // sh = 128 - p. This is the number of low bits in the 128-bit C-limb form
  // that are NOT part of the p-bit significand. sh is in [1, 63].
  // Ref: mpfr/src/add1sp.c L370 — sh = 2 * GMP_NUMB_BITS - p.
  // -------------------------------------------------------------------------
  const sh = 2n * GMP_NUMB_BITS - p;  // sh in [1, 63]
  const mask = (1n << sh) - 1n;  // MPFR_LIMB_MASK(sh)

  const sign: Sign = b.sign;  // both operands have the same sign

  // Convert TS-schema mant (MSB-aligned to p bits) to C 2-limb form
  // (MSB-aligned to 128 bits): climbMant = mant << sh.
  // bp1 = high 64 bits (MSW), bp0 = low 64 bits (LSW).
  // Ref: spec.json "converting TS-schema mantissas to C-limb form".
  let bp1 = b.mant >> (p - GMP_NUMB_BITS);  // mant >> (p - 64) = high 64 bits when shifted left by sh
  let bp0 = (b.mant << sh) & MASK64;         // low 64 bits of mant << sh
  let cp1 = c.mant >> (p - GMP_NUMB_BITS);
  let cp0 = (c.mant << sh) & MASK64;

  let bx: bigint = b.exp;
  const cx: bigint = c.exp;

  let ap1: bigint;
  let ap0: bigint;
  let rb: bigint;
  let sb: bigint;

  if (bx === cx) {
    // -----------------------------------------------------------------------
    // Case A: equal exponents.
    // Since bp[1], cp[1] >= MPFR_LIMB_HIGHBIT, a carry always occurs.
    // Ref: mpfr/src/add1sp.c L379-L390.
    //
    //   a0 = bp[0] + cp[0]
    //   a1 = bp[1] + cp[1] + (a0 < bp[0])   [carry from low limb]
    //   a0 = (a0 >> 1) | (a1 << 63)          [shift right, fold carry bit]
    //   ap[1] = HIGHBIT | (a1 >> 1)
    //   bx++
    //   rb = a0 & (1 << (sh-1))
    //   ap[0] = a0 ^ rb
    //   sb = 0
    // -----------------------------------------------------------------------
    const rawA0 = bp0 + cp0;
    const carry0 = rawA0 >> GMP_NUMB_BITS;  // 0 or 1
    const a0_64 = rawA0 & MASK64;
    const rawA1 = bp1 + cp1 + carry0;
    // rawA1 can overflow 64 bits but we only need the low 64 bits here.
    // The high bit of rawA1 (bit 64) is the carry-out; since bp[1] and cp[1]
    // both have MSB set, the sum is >= 2^64, so the carry always occurs.
    const a1_64 = rawA1 & MASK64;
    // Shift 128-bit value (a1_64 : a0_64) right by 1:
    //   new_a0 = (a0_64 >> 1) | (a1_64 << 63)
    //   new_a1 = (carry_out_from_a1 << 63) | (a1_64 >> 1)
    // But since carry always occurs (MPFR comment), ap[1] = HIGHBIT | (a1 >> 1).
    // Ref: mpfr/src/add1sp.c L384-L387.
    const a0_shifted = ((a0_64 >> 1n) | (a1_64 << 63n)) & MASK64;
    bx += 1n;
    rb = a0_shifted & (1n << (sh - 1n));
    ap1 = LIMB_HIGHBIT | (a1_64 >> 1n);
    ap0 = a0_shifted ^ rb;  // clear round bit in stored limb
    sb = 0n;  // b+c fits on p+1 bits, no sticky bits
  } else {
    // -----------------------------------------------------------------------
    // Ensure bx >= cx by swapping if necessary.
    // Ref: mpfr/src/add1sp.c L393-L399.
    // -----------------------------------------------------------------------
    if (bx < cx) {
      // Swap b and c.
      bx = cx;  // bx now holds the larger exponent (which was cx)
      const tmp1 = bp1; bp1 = cp1; cp1 = tmp1;
      const tmp0 = bp0; bp0 = cp0; cp0 = tmp0;
    }
    // Now bx > cx (the original larger exponent is in bx,
    // the original smaller exponent is in cx but we track d = bx - cx).
    // Note: after swap, bx = max(b.exp, c.exp), and the original min exponent
    // is still in cx (since we only changed bx, not cx).
    // Wait — if b.exp < c.exp, after swap bx = c.exp and cx = c.exp... no.
    // cx was set to c.exp above and never changed. If b.exp < c.exp,
    // then after setting bx = cx (= c.exp), we have bx = c.exp > b.exp.
    // cx is still c.exp. So d = bx - cx = 0. That's wrong.
    //
    // Fix: we need to track the actual smaller exponent properly.
    // After the swap, "b" logically is whichever had the larger exponent.
    // The smaller exponent is the other one. Let's compute d directly.
    //
    // Actually: before the potential swap, let's capture d = |b.exp - c.exp|.
    // Then after the swap (if any), bx holds max(b.exp, c.exp),
    // and d = bx - min(b.exp, c.exp).
    //
    // Rewrite: compute d = bx - (original min exponent).
    // But we already modified bx. We need to recompute.
    //
    // The simplest correct approach: compute d from original exponents.
    const d: bigint = bx > cx ? bx - cx : cx - bx;  // Wait, bx may have changed above.
    // Actually after the `if (bx < cx) { bx = cx; ... }` block,
    // if we swapped: bx is now the old cx (= c.exp), and cx is still c.exp.
    // So bx === cx, giving d = 0. That is wrong.
    //
    // The real fix: save the original b.exp and c.exp before any swap,
    // and compute d from those. Let's use b.exp and c.exp directly.
    // (We've already assigned bx = b.exp at the top. But after swap,
    // bx might equal c.exp. We'll use absolute exponents from inputs.)
    void d;  // discard above

    // Recompute correctly: after the swap (if any), bx holds the LARGER exponent.
    // The smaller exponent is the minimum of b.exp and c.exp.
    const smallExp = b.exp < c.exp ? b.exp : c.exp;
    const d2: bigint = bx - smallExp;  // bx is now the larger one
    // d2 > 0 since bx != smallExp (they were unequal in this else branch)

    if (d2 < GMP_NUMB_BITS) {
      // -------------------------------------------------------------------
      // Case B1: 0 < d < 64.
      // Ref: mpfr/src/add1sp.c L403-L421.
      //
      //   sb = cp[0] << (64 - d)          [bits from cp[-1] after shift]
      //   a0 = bp[0] + ((cp[1] << (64-d)) | (cp[0] >> d))
      //   a1 = bp[1] + (cp[1] >> d) + (a0 < bp[0])    [carry detection]
      //   if (a1 < bp[1])  [carry in high word — true overflow]
      //     goto exponent_shift
      //   else
      //     ap[1] = a1
      //   rb = a0 & (1 << (sh-1))
      //   sb |= (a0 & mask) ^ rb
      //   ap[0] = a0 & ~mask
      // -------------------------------------------------------------------
      sb = (cp0 << (GMP_NUMB_BITS - d2)) & MASK64;
      const aligned = ((cp1 << (GMP_NUMB_BITS - d2)) | (cp0 >> d2)) & MASK64;
      const rawA0b = bp0 + aligned;
      const carry_a0 = rawA0b >> GMP_NUMB_BITS;  // 0 or 1 (a0 < bp[0] in C means overflow)
      const a0 = rawA0b & MASK64;
      const rawA1b = bp1 + (cp1 >> d2) + carry_a0;
      const a1 = rawA1b & MASK64;

      if (rawA1b > MASK64) {
        // carry in high word: a1 < bp[1] in C's unsigned 64-bit arithmetic
        // means the high word overflowed (wrapped around).
        // Ref: mpfr/src/add1sp.c L408-L416 — goto exponent_shift.
        //   sb |= a0 & MPFR_LIMB_ONE
        //   a0 = (a1 << 63) | (a0 >> 1)
        //   ap[1] = HIGHBIT | (a1 >> 1)
        //   bx++
        sb = (sb | (a0 & 1n)) & MASK64;
        const a0_new = ((a1 << 63n) | (a0 >> 1n)) & MASK64;
        ap1 = LIMB_HIGHBIT | (a1 >> 1n);
        bx += 1n;
        rb = a0_new & (1n << (sh - 1n));
        sb = (sb | ((a0_new & mask) ^ rb)) & MASK64;
        ap0 = a0_new & ~mask;
      } else {
        // No carry in high word.
        ap1 = a1;
        rb = a0 & (1n << (sh - 1n));
        sb = (sb | ((a0 & mask) ^ rb)) & MASK64;
        ap0 = a0 & ~mask;
      }
    } else if (d2 < 2n * GMP_NUMB_BITS) {
      // -------------------------------------------------------------------
      // Case B2: 64 <= d < 128.
      // Ref: mpfr/src/add1sp.c L423-L434.
      //
      //   sb = (d == 64) ? cp[0] : cp[0] | (cp[1] << (128-d))
      //   a0 = bp[0] + (cp[1] >> (d - 64))
      //   a1 = bp[1] + (a0 < bp[0])    [carry detection]
      //   if (a1 == 0)   [overflow wrapped — carry out of high word]
      //     goto exponent_shift
      //   rb = a0 & (1 << (sh-1))
      //   sb |= (a0 & mask) ^ rb
      //   ap[0] = a0 & ~mask
      //   ap[1] = a1
      // -------------------------------------------------------------------
      if (d2 === GMP_NUMB_BITS) {
        sb = cp0;
      } else {
        // d > 64: cp[0] | (cp[1] << (128 - d)), truncated to 64 bits.
        sb = (cp0 | ((cp1 << (2n * GMP_NUMB_BITS - d2)) & MASK64)) & MASK64;
      }
      const aligned2 = cp1 >> (d2 - GMP_NUMB_BITS);
      const rawA0c = bp0 + aligned2;
      const carry_a0c = rawA0c >> GMP_NUMB_BITS;  // 0 or 1 (a0 < bp[0])
      const a0 = rawA0c & MASK64;
      const rawA1c = bp1 + carry_a0c;
      const a1 = rawA1c & MASK64;

      if (rawA1c > MASK64) {
        // a1 overflowed: a1 == 0 in C's 64-bit arithmetic (wrapped to 0).
        // goto exponent_shift:
        //   sb |= a0 & MPFR_LIMB_ONE
        //   a0 = (a1 << 63) | (a0 >> 1)   but a1=0 after wrap, so a0 = a0 >> 1
        //   ap[1] = HIGHBIT | (a1 >> 1)     = HIGHBIT | 0 = HIGHBIT
        //   bx++
        // Wait — in exponent_shift, a1 is the post-wrap value (which is 0 in
        // this case). Let me re-read the C code.
        //
        // The C `goto exponent_shift` shares code with the d<64 case.
        // At exponent_shift (L410-L416):
        //   sb |= a0 & MPFR_LIMB_ONE;
        //   a0 = (a1 << (GMP_NUMB_BITS - 1)) | (a0 >> 1);
        //   ap[1] = MPFR_LIMB_HIGHBIT | (a1 >> 1);
        //   bx++;
        // Here a1 is the 64-bit wrapped value. For the d<128 case where
        // a1 wrapped to 0 (rawA1c == 2^64), a1_64 = 0.
        // So: a0_new = (0 << 63) | (a0 >> 1) = a0 >> 1
        //     ap[1] = HIGHBIT | 0 = HIGHBIT
        // Ref: mpfr/src/add1sp.c L410-L416 — exponent_shift label.
        sb = (sb | (a0 & 1n)) & MASK64;
        const a0_new = ((a1 << 63n) | (a0 >> 1n)) & MASK64;
        ap1 = LIMB_HIGHBIT | (a1 >> 1n);
        bx += 1n;
        rb = a0_new & (1n << (sh - 1n));
        sb = (sb | ((a0_new & mask) ^ rb)) & MASK64;
        ap0 = a0_new & ~mask;
      } else {
        ap1 = a1;
        rb = a0 & (1n << (sh - 1n));
        sb = (sb | ((a0 & mask) ^ rb)) & MASK64;
        ap0 = a0 & ~mask;
      }
    } else {
      // -------------------------------------------------------------------
      // Case B3: d >= 128.
      // Ref: mpfr/src/add1sp.c L436-L442.
      //
      //   ap[0] = bp[0]; ap[1] = bp[1]; rb = 0; sb = 1 (c != 0).
      // -------------------------------------------------------------------
      ap0 = bp0;
      ap1 = bp1;
      rb = 0n;
      sb = 1n;
    }
  }

  // -------------------------------------------------------------------------
  // Overflow check (pre-rounding).
  // Ref: mpfr/src/add1sp.c L446-L447.
  // -------------------------------------------------------------------------
  if (bx > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
  }

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: mpfr/src/add1sp.c L449-L479.
  //
  // Ternary: sign of (rounded - exact).
  //   truncate -> ternary = -sign  (C: MPFR_RET(-MPFR_SIGN(a)))
  //   add_one_ulp -> ternary = +sign  (C: MPFR_RET(MPFR_SIGN(a)))
  // -------------------------------------------------------------------------

  if (rb === 0n && sb === 0n) {
    // Exact.
    // Ref: mpfr/src/add1sp.c L450 — `if ((rb == 0 && sb == 0) ...) MPFR_RET(0)`.
    const value = buildNormal2(p, sh, sign, bx, ap1!, ap0!);
    return { value, ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // Ref: mpfr/src/add1sp.c L452-L457.
    // Truncate if rb == 0, OR (sb == 0 AND LSB of result == 0).
    // "bit at position sh of ap[0]" = ap0 & (MPFR_LIMB_ONE << sh).
    const lsbBit = ap0! & (1n << sh);
    if (rb === 0n || (sb === 0n && lsbBit === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    // Ref: mpfr/src/add1sp.c L459-L462.
    doAddOneUlp = false;
  } else {
    // Round away from zero.
    // Ref: mpfr/src/add1sp.c L464-L478.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // Truncate: ternary = -sign.
    // Ref: mpfr/src/add1sp.c L462 — MPFR_RET(-MPFR_SIGN(a)).
    const value = buildNormal2(p, sh, sign, bx, ap1!, ap0!);
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value, ternary };
  }

  // add_one_ulp:
  //   ap[0] += MPFR_LIMB_ONE << sh
  //   ap[1] += (ap[0] == 0)   [carry from low limb]
  //   if ap[1] == 0: overflow -> ap[1] = HIGHBIT, bx++, check emax
  // Ref: mpfr/src/add1sp.c L467-L478.
  const newAp0 = (ap0! + (1n << sh)) & MASK64;
  let newAp1 = (ap1! + (newAp0 === 0n ? 1n : 0n)) & MASK64;
  let newBx = bx;

  if (newAp1 === 0n) {
    // Mantissa overflowed: ap[1] wrapped to 0.
    // Ref: mpfr/src/add1sp.c L469-L477.
    newAp1 = LIMB_HIGHBIT;
    if (bx + 1n <= EMAX_DEFAULT) {
      newBx = bx + 1n;
    } else {
      return mpfr_overflow(p, rnd, sign);
    }
  }

  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  const value = buildNormal2(p, sh, sign, newBx, newAp1, newAp0);
  return { value, ternary };
}
