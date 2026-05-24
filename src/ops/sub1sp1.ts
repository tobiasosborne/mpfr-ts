/**
 * ops/sub1sp1.ts — pure-TS port of MPFR's `mpfr_sub1sp1`.
 *
 * Same-precision same-sign subtraction fast path for p < GMP_NUMB_BITS (i.e.
 * p in [1, 63] on x86_64), where sh = 64 - p > 0 (there is trailing padding).
 * The dispatcher `mpfr_sub1sp` routes here when both operands share the same
 * precision and that precision is strictly less than 64 bits.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_sub1sp1(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                            mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_sub1sp1(b, c, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS)
 * ----------------------------------------------------
 *
 *   - b.kind === 'normal' and c.kind === 'normal'
 *   - b.sign === c.sign (same-sign subtraction; caller ensures this)
 *   - b.prec === c.prec and 1n <= b.prec <= 63n
 *
 * Key difference from sub1sp1n (p == 64)
 * ---------------------------------------
 *
 * Since p < 64, there is a non-zero shift sh = 64 - p. Mantissas are stored
 * in the TS schema MSB-aligned to prec bits, but C arithmetic operates on
 * limbs MSB-aligned to 64 bits. So:
 *
 *   bp0 = b.mant << sh   (convert from TS to C limb form)
 *   cp0 = c.mant << sh
 *
 * and back:
 *
 *   result_mant = ap0 >> sh
 *
 * Also, the "one ulp" in C-limb form is `MPFR_LIMB_ONE << sh`, not `1`.
 * The LSB of the result (for rounding tie-break) is `ap0 & (1 << sh)`.
 *
 * Algorithm overview
 * ------------------
 *
 * Case A (bx == cx, equal exponents): exact subtraction.
 *   - If zero: return ±0 per RNDD sign rule.
 *   - If borrow (c > b in magnitude): flip sign, use cp[0]-bp[0].
 *   - Count leading zeros, left-shift a0, decrement bx. rb = sb = 0.
 *   Note: sh is not used in Case A (the C comment says so).
 *
 * Case B (bx != cx): swap if bx < cx so bx >= cx, compute d = bx - cx.
 *   B1 (d < 64): two's-complement negation sb = -(cp[0] << (64-d));
 *     a0 = bp[0] - (sb!=0) - (cp[0] >> d).
 *     C asserts a0 != 0. Normalize via clz, extract rb/sb from shifted a0.
 *   B2 (d >= 64): result is b minus one ulp(b).
 *     If bp[0] > HIGHBIT: ap[0] = bp[0] - (1 << sh), rb = 1, sb = 1.
 *     Else (bp[0] == HIGHBIT): bx--, ap[0] = ~mask, sb = 1,
 *       rb = (sh > 1) || (d > 64) || (cp[0] == HIGHBIT).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub1sp.c L138-L320 — verbatim C reference body.
 *   - mpfr/src/sub1sp.c L1474-L1476 — dispatcher routing.
 *   - mpfr/src/sub1.c L66-L74 — cancellation-zero sign rule.
 *   - mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 *   - mpfr/src/mpfr-impl.h L1300-L1311 — MPFR_LIMB_HIGHBIT, MPFR_LIMB_MASK.
 *   - src/ops/sub1sp1n.ts — sister op (p == 64); same structure, sh = 0.
 *   - src/ops/add1sp1.ts — companion op (add direction, same prec range).
 *   - src/ops/underflow.ts — delegate on bx < emin.
 *   - src/core.ts — locked schema (MPFR, RoundingMode, Result, Ternary, Sign).
 *   - CLAUDE.md "Signed zero is real" — RNDD cancellation gives -0.
 *   - CLAUDE.md "Ternary flag is the sign of (rounded - exact)".
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../src/core.ts';
import { MPFRError, negZero, posZero } from '../../src/core.ts';
import { mpfr_underflow } from '../../src/ops/underflow.ts';

// ---------------------------------------------------------------------------
// Constants
// Ref: mpfr/src/mpfr-impl.h L1300-L1311
// ---------------------------------------------------------------------------

/** GMP_NUMB_BITS: limb width on x86_64. */
const GMP_NUMB_BITS = 64n;

/** All 64 bits set: used for mod-2^64 masking. */
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
 * MPFR_LIMB_MASK(s): returns a bitmask with the lowest `s` bits set.
 * Ref: mpfr/src/mpfr-impl.h L1308-L1311.
 *
 * Precondition: 0 <= s <= 64. For s == 0 returns 0n; for s == 64 all bits.
 */
function lmbMask(s: bigint): bigint {
  if (s === 0n) return 0n;
  if (s >= 64n) return LIMB_MASK_64;
  return (1n << s) - 1n;
}

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
 * Returns 0 for v >= LIMB_HIGHBIT (bit 63 set).
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
 * Build a normal MPFR value for p < 64 from the C-limb form (64-bit MSB-aligned).
 * Converts back to TS schema: mant = ap0 >> sh.
 * Ref: spec.json "converting TS-schema mantissas to C-limb form".
 */
function buildNormal(sign: Sign, prec: bigint, exp: bigint, ap0: bigint, sh: bigint): MPFR {
  const mant = ap0 >> sh;
  return {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  } satisfies MPFR;
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign subtraction fast path for p < 64 (i.e. p in [1,63]).
 *
 * Mirrors `mpfr_sub1sp1` from `mpfr/src/sub1sp.c L138-L320`.
 *
 * @mpfrName mpfr_sub1sp1
 *
 * @param b   Minuend; must be kind='normal', 1 <= prec <= 63.
 * @param c   Subtrahend; must be kind='normal', same prec and sign as b.
 * @param rnd Rounding mode (five modes per src/core.ts).
 *
 * @returns `{ value, ternary }` — the correctly-rounded difference and the
 *          ternary flag (sign of rounded − exact).
 *
 * @throws {MPFRError} EPREC for invalid kind/prec; EROUND for unknown rnd.
 */
export function mpfr_sub1sp1(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/sub1sp.c L155 — MPFR_ASSERTD(p < GMP_NUMB_BITS).
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp1: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp1: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_sub1sp1: b.prec (${b.prec}) !== c.prec (${c.prec})`);
  }
  if (b.prec < 1n || b.prec > 63n) {
    throw new MPFRError('EPREC', `mpfr_sub1sp1: prec must be in [1,63], got ${b.prec}`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_sub1sp1: b.sign (${b.sign}) !== c.sign (${c.sign})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_sub1sp1: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Setup: sh = 64 - p > 0 since p < 64.
  // Convert TS-schema mantissas (MSB-aligned to prec bits) to C-limb form
  // (MSB-aligned to 64 bits): bp0 = b.mant << sh.
  // Ref: mpfr/src/sub1sp.c L143-L148, L203.
  // -------------------------------------------------------------------------

  const p = b.prec;
  const sh = GMP_NUMB_BITS - p;          // sh > 0 always for p < 64
  const mask = lmbMask(sh);               // MPFR_LIMB_MASK(sh)

  // Convert to 64-bit C-limb form.
  let bp0: bigint = b.mant << sh;
  let cp0: bigint = c.mant << sh;

  let bx: bigint = b.exp;
  const cx0: bigint = c.exp;
  let cx: bigint = cx0;

  // Result sign: starts as b.sign, may be flipped on borrow or swap.
  let resultSign: Sign = b.sign;

  let ap0: bigint;  // result limb (64-bit C form, MSB-aligned)
  let rb: bigint;   // round bit (nonzero <=> set)
  let sb: bigint;   // sticky bit (nonzero <=> any lost bit was set)

  // -------------------------------------------------------------------------
  // Case A: bx == cx (equal exponents, exact subtraction)
  // Ref: mpfr/src/sub1sp.c L157-L186.
  // Note: sh is not used in Case A (the C comment at L185 says so).
  // -------------------------------------------------------------------------

  if (bx === cx) {
    if (bp0 === cp0) {
      // Complete cancellation: result is zero.
      // Sign rule: RNDD → -0, all others → +0.
      // Ref: mpfr/src/sub1sp.c L159-L167 and mpfr/src/sub1.c L66-L74.
      if (rnd === 'RNDD') {
        return { value: negZero(p), ternary: 0 };
      } else {
        return { value: posZero(p), ternary: 0 };
      }
    }

    let a0: bigint;
    if (cp0 > bp0) {
      // Borrow: |c| > |b|, so result has opposite sign.
      // Ref: mpfr/src/sub1sp.c L168-L172.
      a0 = cp0 - bp0;
      resultSign = (b.sign === 1 ? -1 : 1) as Sign;
    } else {
      // bp[0] > cp[0]: result has same sign.
      // Ref: mpfr/src/sub1sp.c L173-L177.
      a0 = bp0 - cp0;
      resultSign = b.sign;
    }

    // a0 != 0 (MPFR_ASSERTD at L180).
    // Count leading zeros, left-shift, decrement bx.
    // Ref: mpfr/src/sub1sp.c L181-L184.
    const cnt = clz64(a0);
    ap0 = (a0 << cnt) & LIMB_MASK_64;
    bx = bx - cnt;
    rb = 0n;
    sb = 0n;

  } else {
    // -----------------------------------------------------------------------
    // Case B: bx != cx
    // Ref: mpfr/src/sub1sp.c L189-L265.
    // -----------------------------------------------------------------------

    // Swap b and c if bx < cx so that bx > cx.
    // Ref: mpfr/src/sub1sp.c L189-L200.
    if (bx < cx) {
      const tx = bx; bx = cx; cx = tx;
      const tp = bp0; bp0 = cp0; cp0 = tp;
      // MPFR_SET_OPPOSITE_SIGN(a, b): result sign flips.
      resultSign = (b.sign === 1 ? -1 : 1) as Sign;
    } else {
      // MPFR_SET_SAME_SIGN(a, b).
      resultSign = b.sign;
    }

    // d = bx - cx > 0 (unsigned in C: mpfr_uexp_t).
    // Ref: mpfr/src/sub1sp.c L202.
    const d: bigint = bx - cx;

    if (d < GMP_NUMB_BITS) {
      // -----------------------------------------------------------------------
      // Case B1: 0 < d < 64.
      // Ref: mpfr/src/sub1sp.c L205-L234.
      //
      // sb = -(cp[0] << (64 - d))  [two's-complement negation of neglected part]
      // a0 = bp[0] - (sb != 0) - (cp[0] >> d)
      //
      // C asserts a0 > 0:
      //   a) if d >= 2: a0 >= 2^(w-1) - (2^(w-2)-1) with w=64, so a0-1 >= 2^(w-2).
      //   b) if d == 1: since p < GMP_NUMB_BITS, sh > 0, so sb = 0.
      // -----------------------------------------------------------------------

      // Two's-complement negation of the shifted-out bits:
      // sb = -(cp[0] << (64-d)) mod 2^64
      // Ref: mpfr/src/sub1sp.c L207 — `sb = - (cp[0] << (GMP_NUMB_BITS - d));`
      const shifted_cp = (cp0 << (GMP_NUMB_BITS - d)) & LIMB_MASK_64;
      sb = shifted_cp === 0n ? 0n : (LIMB_MASK_64 + 1n - shifted_cp) & LIMB_MASK_64;

      // a0 = bp[0] - (sb != 0) - (cp[0] >> d)
      // Ref: mpfr/src/sub1sp.c L217.
      const borrow = sb !== 0n ? 1n : 0n;
      const cp_shifted = cp0 >> d;
      // Since C uses unsigned 64-bit subtraction, we mask to 64 bits.
      let a0 = (bp0 - borrow - cp_shifted) & LIMB_MASK_64;

      // C asserts a0 > 0 (MPFR_ASSERTD at L223).
      // Normalize: count leading zeros, shift a0 left, shift sb left.
      // Ref: mpfr/src/sub1sp.c L224-L233.
      const cnt = clz64(a0);
      if (cnt > 0n) {
        // a0 = (a0 << cnt) | (sb >> (64 - cnt))
        // Ref: mpfr/src/sub1sp.c L226.
        a0 = ((a0 << cnt) | (sb >> (GMP_NUMB_BITS - cnt))) & LIMB_MASK_64;
        sb = (sb << cnt) & LIMB_MASK_64;
      }
      bx = bx - cnt;

      // sh > 0 since p < 64 (MPFR_ASSERTD at L230).
      // rb = a0 & (MPFR_LIMB_ONE << (sh - 1))
      // sb |= (a0 & mask) ^ rb
      // ap[0] = a0 & ~mask
      // Ref: mpfr/src/sub1sp.c L231-L233.
      rb = a0 & (1n << (sh - 1n));
      sb = (sb | ((a0 & mask) ^ rb)) & LIMB_MASK_64;
      ap0 = a0 & ~mask & LIMB_MASK_64;

    } else {
      // -----------------------------------------------------------------------
      // Case B2: d >= 64.
      // Ref: mpfr/src/sub1sp.c L235-L265.
      //
      // The entire c significand is below b's precision window.
      // Result is b minus one ulp(b) in C-limb form.
      // One ulp(b) in C-limb form = MPFR_LIMB_ONE << sh = 1 << sh.
      // -----------------------------------------------------------------------

      if (bp0 > LIMB_HIGHBIT) {
        // bp[0] > MPFR_LIMB_HIGHBIT: result is bp[0] - (1 << sh).
        // rb = 1, sb = 1 (ulp(b) - c satisfies 1/2 ulp(b) < ulp(b)-c < ulp(b)).
        // Ref: mpfr/src/sub1sp.c L237-L243.
        ap0 = (bp0 - (1n << sh)) & LIMB_MASK_64;
        rb = 1n;
        sb = 1n;
      } else {
        // bp[0] == MPFR_LIMB_HIGHBIT: exponent decreases by 1.
        // Ref: mpfr/src/sub1sp.c L244-L264.
        //
        // ap[0] = ~mask  (= 1111...100...0 with sh zeros at the bottom)
        // bx--
        // rb = (sh > 1) || (d > GMP_NUMB_BITS) || (cp[0] == MPFR_LIMB_HIGHBIT)
        // sb = 1
        //
        // Note from C comments (L247-L263): when p = GMP_NUMB_BITS-1, d = GMP_NUMB_BITS
        // and c0 = HIGHBIT, we have rb=1 but sb=0 is the "true" sticky. But the even
        // rounding rule would round up anyway, so setting sb=1 gives the correct result.
        rb = (sh > 1n || d > GMP_NUMB_BITS || cp0 === LIMB_HIGHBIT) ? 1n : 0n;
        // sb = 1 always in this branch.
        sb = 1n;
        ap0 = ~mask & LIMB_MASK_64;
        bx = bx - 1n;
      }
    }
  }

  // -------------------------------------------------------------------------
  // Underflow check (post-subtraction exponent may be below emin).
  // Ref: mpfr/src/sub1sp.c L275-L289.
  //
  // Since b and c have same precision p, b-c is a multiple of 2^(emin-p),
  // so if bx < emin the subtraction (with unbounded exponent range) is exact
  // and rb = sb = 0. bx is therefore the exponent after rounding.
  //
  // For RNDN, mpfr_underflow always rounds away, so for |a| <= 2^(emin-2)
  // we must switch to RNDZ:
  //   (a) bx < emin - 1
  //   (b) bx == emin - 1 and ap[0] == MPFR_LIMB_HIGHBIT (necessarily rb=sb=0)
  // -------------------------------------------------------------------------

  if (bx < EMIN_DEFAULT) {
    let effectiveRnd = rnd;
    if (rnd === 'RNDN' &&
        (bx < EMIN_DEFAULT - 1n || ap0 === LIMB_HIGHBIT)) {
      // MPFR_ASSERTD(rb == 0 && sb == 0) holds here per C invariant.
      effectiveRnd = 'RNDZ';
    }
    return mpfr_underflow(p, effectiveRnd, resultSign);
  }

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: mpfr/src/sub1sp.c L291-L320.
  //
  // Ternary convention: sign of (rounded - exact).
  //   truncate  → ternary = -resultSign  (C: MPFR_RET(-MPFR_SIGN(a)))
  //   add_one_ulp → ternary = +resultSign  (C: MPFR_RET(MPFR_SIGN(a)))
  // -------------------------------------------------------------------------

  if (rb === 0n && sb === 0n) {
    // Exact result.
    // Ref: mpfr/src/sub1sp.c L292-L293.
    return { value: buildNormal(resultSign, p, bx, ap0, sh), ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // Round to nearest, ties to even.
    // Ref: mpfr/src/sub1sp.c L294-L299.
    //
    // Truncate if:
    //   rb == 0  (below midpoint)
    //   OR rb != 0 AND sb == 0 AND LSB of result == 0 (tie → even)
    //
    // LSB of result in C-limb form is bit sh (the lowest set bit of
    // the p-bit mantissa), i.e. ap0 & (1 << sh).
    // Ref: sub1sp.c L296 — `(ap[0] & (MPFR_LIMB_ONE << sh)) == 0`
    const lsb = ap0 & (1n << sh);
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, resultSign)) {
    // Truncate toward zero.
    // Ref: mpfr/src/sub1sp.c L301-L304.
    doAddOneUlp = false;
  } else {
    // Round away from zero.
    // Ref: mpfr/src/sub1sp.c L306-L319.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // Truncate branch: ternary = -resultSign.
    // Ref: mpfr/src/sub1sp.c L304 — `MPFR_RET(-MPFR_SIGN(a))`.
    const ternary: Ternary = (resultSign === 1 ? -1 : 1) as Ternary;
    return { value: buildNormal(resultSign, p, bx, ap0, sh), ternary };
  }

  // add_one_ulp branch.
  // Ref: mpfr/src/sub1sp.c L309-L319.
  //
  // ap[0] += MPFR_LIMB_ONE << sh
  // If ap[0] == 0 (overflow): ap[0] = HIGHBIT, bx++.
  // Note: bx+1 cannot exceed emax since |a| <= |b|.
  let newAp0 = (ap0 + (1n << sh)) & LIMB_MASK_64;
  let newBx = bx;

  if (newAp0 === 0n) {
    // Mantissa overflow: ap[0] = HIGHBIT, increment exponent.
    // Ref: mpfr/src/sub1sp.c L311-L316.
    newAp0 = LIMB_HIGHBIT;
    newBx = bx + 1n;
    // C asserts bx+1 <= emax since |a| <= |b|.
  }

  const ternary: Ternary = (resultSign === 1 ? 1 : -1) as Ternary;
  return { value: buildNormal(resultSign, p, newBx, newAp0, sh), ternary };
}
