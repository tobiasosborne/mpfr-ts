/**
 * ops/add1sp1.ts — pure-TS port of MPFR's `mpfr_add1sp1`.
 *
 * The same-precision, same-sign addition fast path for `p < 64` bits
 * (single-limb operands). This is the most frequently hit branch in
 * MPFR's arithmetic kernel: the dispatcher `mpfr_add1sp` (add1sp.c
 * L856-L894) routes here when all three operands share the same prec
 * and that prec is < GMP_NUMB_BITS (= 64 on x86_64).
 *
 * C signature
 * -----------
 *
 *   static int mpfr_add1sp1(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                            mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_add1sp1(b, c, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS)
 * ----------------------------------------------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.sign === c.sign` (same-sign addition; caller ensures this)
 *   - `b.prec === c.prec` and `1n <= b.prec <= 63n`
 *
 * The output precision and sign are both taken from `b` (the dispatcher
 * sets `a.sign = b.sign` before calling, and `a.prec = b.prec`).
 *
 * Algorithm
 * ---------
 *
 * All arithmetic runs in C "limb space": mantissas are MSB-aligned to
 * 64 bits (C convention), not MSB-aligned to `prec` bits (TS schema
 * convention).  The conversion is:
 *
 *   b_limb = b.mant << sh   where sh = 64 - prec
 *
 * and back:
 *
 *   result_mant = ap0 >> sh
 *
 * The algorithm has four sub-cases:
 *
 *   Case A:  bx == cx  (exponents equal)
 *   Case B1: 0 < d < sh  (difference < sh; no bit loss in alignment)
 *   Case B2: sh <= d < 64  (some bits lost in alignment — sticky needed)
 *   Case B3: d >= 64  (c rounds to zero relative to b — only sticky)
 *
 * Overflow uses the EMAX_DEFAULT constant matching `__gmpfr_emax`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add1sp.c L128-L252 — the verbatim C reference body.
 *   - mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 *   - mpfr/src/mpfr-impl.h L1300-L1311 — MPFR_LIMB_HIGHBIT, MPFR_LIMB_MASK.
 *   - mpfr/src/mpfr.h L231 — MPFR_EMAX_DEFAULT = 2^30 - 1.
 *   - src/ops/overflow.ts — delegate for exponent overflow.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign
 *     of (rounded - exact), not 0/1."
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../core.ts';
import { MPFRError } from '../core.ts';
import { mpfr_overflow } from './overflow.ts';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** 64-bit limb width: GMP_NUMB_BITS on x86_64. */
const GMP_NUMB_BITS = 64n;

/** 64-bit mask: all 64 bits set. */
const LIMB_MASK_64 = (1n << GMP_NUMB_BITS) - 1n;

/**
 * MPFR_LIMB_HIGHBIT = 1 << 63 — the MSB of a 64-bit limb.
 * Ref: mpfr/src/mpfr-impl.h L1301.
 */
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
 * MPFR_LIMB_MASK(s): returns a bitmask with the lowest `s` bits set.
 * Ref: mpfr/src/mpfr-impl.h L1308-L1311.
 *
 * Precondition: 0 <= s <= 64.  For s == 0 returns 0n; for s == 64 all bits.
 */
function lmbMask(s: bigint): bigint {
  if (s === 0n) return 0n;
  if (s >= 64n) return LIMB_MASK_64;
  return (1n << s) - 1n;
}

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ (mpfr-impl.h L1233-L1234):
 * "does `rnd` round toward zero with respect to the sign?"
 *
 *   - RNDZ always rounds toward zero.
 *   - RNDD rounds toward -∞; that's toward zero when sign > 0 (positive).
 *   - RNDU rounds toward +∞; that's toward zero when sign < 0 (negative).
 *   - RNDN and RNDA never round toward zero unconditionally.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign addition fast path for `p < 64`.
 *
 * Mirrors `mpfr_add1sp1` from `mpfr/src/add1sp.c L128-L252`.
 *
 * @mpfrName mpfr_add1sp1
 *
 * @param b   First addend; must be `kind='normal'`, `1 <= prec <= 63`.
 * @param c   Second addend; must be `kind='normal'`, same `prec` and `sign`.
 * @param rnd Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }` — the correctly-rounded sum and the
 *          ternary flag (sign of rounded − exact).
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_add1sp1(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/add1sp.c L130-L145 — MPFR_ASSERTD checks (elevated to throws).
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp1: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp1: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_add1sp1: b.prec (${b.prec}) !== c.prec (${c.prec})`);
  }
  if (b.prec < 1n || b.prec > 63n) {
    throw new MPFRError('EPREC', `mpfr_add1sp1: prec must be in [1,63], got ${b.prec}`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_add1sp1: b.sign (${b.sign}) !== c.sign (${c.sign})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_add1sp1: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Set up C-mirror variables
  // Ref: mpfr/src/add1sp.c L133-L143.
  // -------------------------------------------------------------------------

  const p = b.prec;                          // mpfr_prec_t p
  const sh = GMP_NUMB_BITS - p;             // sh = 64 - p
  const sign = b.sign;                       // same for both operands

  // Convert TS-schema mant (MSB-aligned to prec bits) to C-limb form
  // (MSB-aligned to 64 bits): bp0 = b.mant << sh.
  // Ref: spec.json "converting TS-schema mantissas … to C-limb form".
  let bp0 = b.mant << sh;
  let cp0 = c.mant << sh;

  // C exponents (both are signed; exponent of 'normal' value in MPFR).
  let bx: bigint = b.exp;
  let cx: bigint = c.exp;

  // -------------------------------------------------------------------------
  // Core arithmetic: three cases
  // Ref: mpfr/src/add1sp.c L147-L210.
  // -------------------------------------------------------------------------

  let ap0: bigint;  // the result limb (64-bit C-form)
  let rb: bigint;   // round bit (nonzero <=> round bit is set)
  let sb: bigint;   // sticky bit (nonzero <=> any lost bit was set)

  if (bx === cx) {
    // -----------------------------------------------------------------------
    // Case A: equal exponents.
    // Ref: mpfr/src/add1sp.c L148-L159.
    //
    // a0 = (bp[0] >> 1) + (cp[0] >> 1)  -- halve both, then add
    // This avoids a carry into bit 64.
    // bx++ (the result magnitude is in [2^bx, 2^{bx+1})).
    // rb = a0 & (1 << (sh-1))  -- round bit is bit (sh-1).
    // ap[0] = a0 ^ rb           -- zero the round bit in the stored limb.
    // sb = 0                    -- b+c fits on p+1 bits, no sticky bits.
    // -----------------------------------------------------------------------
    const a0 = (bp0 >> 1n) + (cp0 >> 1n);
    bx = bx + 1n;
    rb = a0 & (1n << (sh - 1n));
    ap0 = a0 ^ rb;               // clear round bit in stored result
    sb = 0n;
  } else {
    // Ensure bx >= cx (swap if needed).
    // Ref: mpfr/src/add1sp.c L162-L168.
    if (bx < cx) {
      const tx = bx; bx = cx; cx = tx;
      const tp = bp0; bp0 = cp0; cp0 = tp;
    }
    // Now bx > cx.
    const d = bx - cx;  // d = bx - cx > 0; mpfr_uexp_t in C (unsigned)
    const mask = lmbMask(sh);  // MPFR_LIMB_MASK(sh)

    if (d < sh) {
      // -------------------------------------------------------------------
      // Case B1: 0 < d < sh.
      // Ref: mpfr/src/add1sp.c L175-L189.
      //
      // c is shifted right by d bits; all of c fits within the p bits
      // (no bits fall off the bottom of the significand).  There may still
      // be a carry.
      // -------------------------------------------------------------------
      let a0 = bp0 + (cp0 >> d);
      if (a0 > LIMB_MASK_64) {
        // carry: a0 overflowed 64 bits
        a0 = a0 & LIMB_MASK_64; // keep low 64 bits
        // C: a0 = MPFR_LIMB_HIGHBIT | (a0 >> 1)
        // but a0 here is bp[0]+cp[0]>>d which overflowed; in C the carry flag
        // detects this as a0 < bp[0]. The post-overflow a0 in C is the low 64
        // bits of the true sum. Since there was a carry the high bit of a0
        // (LIMB_HIGHBIT) is set (the sum's bit 64 became the carry, so the
        // resulting 64-bit number has its MSB set after right shift).
        // Re-read: C does: a0 += ...; if (a0 < bp[0]) { a0 = HB | (a0>>1); bx++ }
        // The C 64-bit wrap gives: a0_wrapped = (bp + cp>>d) mod 2^64.
        // Since there was a carry, the true sum = 2^64 + a0_wrapped.
        // The correct procedure: a0 = LIMB_HIGHBIT | (a0_wrapped >> 1); bx++
        a0 = LIMB_HIGHBIT | (a0 >> 1n);
        bx = bx + 1n;
      }
      rb = a0 & (1n << (sh - 1n));
      sb = (a0 & mask) ^ rb;
      ap0 = a0 & ~mask;
    } else if (d < GMP_NUMB_BITS) {
      // -------------------------------------------------------------------
      // Case B2: sh <= d < 64.
      // Ref: mpfr/src/add1sp.c L190-L203.
      //
      // The shift amount d >= sh means some bits of cp fall below the
      // precision window entirely and contribute to the sticky bit.
      // -------------------------------------------------------------------
      // sb gets the bits that were shifted out by the alignment:
      // cp[0] << (64 - d) gives the bits of cp that fall below bit 0
      // of bp (in C: cp[-1] after a 2-limb shift — but for single-limb
      // case there's no lower limb, so these bits are pure sticky).
      sb = (cp0 << (GMP_NUMB_BITS - d)) & LIMB_MASK_64;
      let a0 = (bp0 + (cp0 >> d)) & LIMB_MASK_64;
      const bp0_64 = bp0 & LIMB_MASK_64;
      // detect carry: a0 < bp0 in 64-bit arithmetic
      const carry = (a0 < bp0_64) ? 1n : 0n;
      if (carry !== 0n) {
        sb = (sb | (a0 & 1n)) & LIMB_MASK_64;  // sb |= a0 & MPFR_LIMB_ONE
        a0 = LIMB_HIGHBIT | (a0 >> 1n);
        bx = bx + 1n;
      }
      rb = a0 & (1n << (sh - 1n));
      sb = (sb | ((a0 & mask) ^ rb)) & LIMB_MASK_64;
      ap0 = a0 & ~mask;
    } else {
      // -------------------------------------------------------------------
      // Case B3: d >= 64.
      // Ref: mpfr/src/add1sp.c L204-L209.
      //
      // c is so much smaller than b that it contributes only as a sticky
      // bit.  The result is b unchanged (except rounding).
      // -------------------------------------------------------------------
      ap0 = bp0;
      rb = 0n;
      sb = 1n;  // c != 0 (it's a normal value)
    }
  }

  // -------------------------------------------------------------------------
  // Overflow check (pre-rounding).
  // Ref: mpfr/src/add1sp.c L217-L218.
  // -------------------------------------------------------------------------

  if (bx > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
  }

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: mpfr/src/add1sp.c L220-L251.
  //
  // The convention for the sign of ternary is "sign of (rounded − exact)":
  //   truncate → rounded < exact → ternary = −sign  (C: MPFR_RET(-MPFR_SIGN(a)))
  //   add_one_ulp → rounded > exact → ternary = +sign  (C: MPFR_RET(MPFR_SIGN(a)))
  //
  // MPFR_RET(t) sets ternary to t directly. MPFR_SIGN(a) is +1 for positive.
  // So:
  //   truncate  → ternary = -sign
  //   add_one_ulp → ternary = +sign
  // -------------------------------------------------------------------------

  if (rb === 0n && sb === 0n) {
    // Exact result: no rounding needed.
    // Ref: mpfr/src/add1sp.c L221 — `if ((rb == 0 && sb == 0) ...) MPFR_RET(0)`.
    const value: MPFR = buildNormal(p, sign, bx, ap0, sh);
    return { value, ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // -----------------------------------------------------------------------
    // Round to nearest (ties to even).
    // Ref: mpfr/src/add1sp.c L223-L231.
    //
    // Truncate if:
    //   rb == 0  (below midpoint)
    //   OR (rb != 0 AND sb == 0 AND result bit at position sh is 0)  (tie → even)
    //
    // add_one_ulp otherwise.
    //
    // Note: "bit at position sh" of ap0 means the least significant bit of
    // the stored mantissa, since ap0's meaningful bits occupy [sh, 63] and
    // bit sh is the LSB of the p-bit result.
    // Ref: add1sp.c L227: `(ap[0] & (MPFR_LIMB_ONE << sh)) == 0`
    // -----------------------------------------------------------------------
    const lsb = ap0 & (1n << sh);  // MPFR_LIMB_ONE << sh
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    // Truncate toward zero.
    // Ref: mpfr/src/add1sp.c L232-L235.
    doAddOneUlp = false;
  } else {
    // Round away from zero (RNDA, or RNDU when positive, or RNDD when negative).
    // Ref: mpfr/src/add1sp.c L237-L251.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // truncate branch: ternary = -sign
    // Ref: add1sp.c L235 — `MPFR_RET(-MPFR_SIGN(a))`.
    const value: MPFR = buildNormal(p, sign, bx, ap0, sh);
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value, ternary };
  }

  // add_one_ulp branch.
  // Ref: mpfr/src/add1sp.c L239-L250.
  //
  // ap[0] += MPFR_LIMB_ONE << sh
  // If ap[0] overflows (== 0 after 64-bit wrap), set ap[0] = LIMB_HIGHBIT,
  // and bump exponent.  If bx+1 > emax, overflow again.

  let newAp0 = (ap0 + (1n << sh)) & LIMB_MASK_64;
  let newBx = bx;

  if (newAp0 === 0n) {
    // Overflow of the mantissa: the add_one_ulp carried all the way out.
    // Ref: add1sp.c L241-L248.
    newAp0 = LIMB_HIGHBIT;
    newBx = bx + 1n;
    if (newBx > EMAX_DEFAULT) {
      return mpfr_overflow(p, rnd, sign);
    }
  }

  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  const value: MPFR = buildNormal(p, sign, newBx, newAp0, sh);
  return { value, ternary };
}

// ---------------------------------------------------------------------------
// buildNormal: convert C-limb form back to TS-schema MPFR.
// ---------------------------------------------------------------------------

/**
 * Convert a C-limb result back to TS-schema MPFR normal value.
 *
 * C stores mantissas MSB-aligned to 64 bits (the `ap[0]` form); the TS
 * schema stores them MSB-aligned to `prec` bits.  So:
 *
 *   mant = ap0 >> sh   (where sh = 64 - prec)
 *
 * The non-precision bits (the low `sh` bits of `ap0`) must be zero by
 * construction (they've been masked in every branch).
 *
 * Ref: spec.json "converting TS-schema mantissas … to C-limb form".
 */
function buildNormal(
  prec: bigint,
  sign: Sign,
  exp: bigint,
  ap0: bigint,
  sh: bigint,
): MPFR {
  const mant = ap0 >> sh;
  return {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  } satisfies MPFR;
}
