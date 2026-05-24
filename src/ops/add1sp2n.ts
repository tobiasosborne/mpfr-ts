/**
 * ops/add1sp2n.ts — pure-TS port of MPFR's `mpfr_add1sp2n`.
 *
 * The same-precision, same-sign addition fast path for `p == 128`
 * exactly (= 2 * GMP_NUMB_BITS on x86_64), two-limb mantissas with
 * sh == 0. The dispatcher `mpfr_add1sp` (add1sp.c L1488-L1491) routes
 * here when `p == 2 * GMP_NUMB_BITS`.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_add1sp2n(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                             mpfr_rnd_t rnd_mode)
 *
 * TS signature
 * ------------
 *
 *   mpfr_add1sp2n(b, c, rnd) -> Result
 *
 * Pre-conditions
 * --------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.sign === c.sign`
 *   - `b.prec === c.prec === 128n`
 *
 * Algorithm
 * ---------
 *
 * Like its sister `mpfr_add1sp2` (65..127 bits), this port uses the
 * bigint extended-precision approach. Since p = 128 = 2*GMP_NUMB_BITS
 * (sh == 0), the rounding point falls at the limb boundary; the C
 * reference handles this via a uniform round/sticky-bit scheme.
 *
 * The mathematical values are:
 *
 *   value(b) = b.sign * b.mant * 2^(b.exp - 128)
 *   value(c) = c.sign * c.mant * 2^(c.exp - 128)
 *
 * Let large be the operand with greater exponent (or b if equal), and
 * small the other. The exact sum at extended precision is:
 *
 *   d = large.exp - small.exp  (>= 0)
 *   S = (large.mant << d) + small.mant    // exact positive bigint
 *
 * S has either p or p+1 bits. If p+1 bits (carry), bx = large.exp + 1;
 * otherwise bx = large.exp. Round S from its actual bit-length down to
 * p bits using roundMantissa.
 *
 * The special case d >= 2*p (>= 256): small is swamped; S = large.mant
 * with round bit from the large/small boundary:
 *   - d == 128: rb = 1, sticky = (cp[0] != 0 || cp[1] > HIGHBIT)
 *   - d > 128: rb = 0, sticky = 1
 * These must be handled explicitly because shifting by d >= 128 in
 * bigint would include the full small.mant (not just the round/sticky).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add1sp.c L482-L611 — the C reference body.
 *   - mpfr/src/add1sp.c L1488-L1491 — dispatcher routing.
 *   - src/ops/sub1sp2n.ts — companion op (sub direction); same approach.
 *   - src/ops/add1sp2.ts — sister op (65..127 bits); same bigint strategy.
 *   - src/internal/mpfr/round_raw.ts — substrate rounding primitive.
 *   - src/ops/overflow.ts — delegate on bx > emax.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is sign of
 *     (rounded - exact), not 0/1."
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../src/core.ts';
import { MPFRError } from '../../src/core.ts';
import { mpfr_overflow } from '../../src/ops/overflow.ts';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/**
 * Default maximum exponent. Mirrors MPFR_EMAX_DEFAULT.
 * Ref: mpfr/src/mpfr.h L231.
 */
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n;

/** Precision for this fast path: p = 2 * GMP_NUMB_BITS = 128. */
const P: bigint = 128n;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Bit length of a positive bigint. Returns 0n for 0. */
function bitLen(x: bigint): bigint {
  if (x <= 0n) return 0n;
  return BigInt(x.toString(2).length);
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign addition fast path for `p == 128`.
 *
 * Mirrors `mpfr_add1sp2n` from `mpfr/src/add1sp.c L482-L611`.
 *
 * @mpfrName mpfr_add1sp2n
 *
 * @param b   First addend; must be `kind='normal'`, `prec === 128n`.
 * @param c   Second addend; must be `kind='normal'`, same `prec` and `sign`.
 * @param rnd Rounding mode.
 *
 * @returns `{ value, ternary }` — correctly-rounded sum and ternary.
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_add1sp2n(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/add1sp.c L485-L495 — MPFR_ASSERTD checks.
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp2n: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp2n: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_add1sp2n: b.prec (${b.prec}) !== c.prec (${c.prec})`);
  }
  if (b.prec !== P) {
    throw new MPFRError('EPREC', `mpfr_add1sp2n: prec must be exactly 128, got ${b.prec}`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_add1sp2n: b.sign (${b.sign}) !== c.sign (${c.sign})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_add1sp2n: unknown rounding mode '${String(rnd)}'`);
  }

  const sign: Sign = b.sign;

  // -------------------------------------------------------------------------
  // Order operands: large has the greater exponent, small the lesser.
  // Ref: mpfr/src/add1sp.c L509-L514 — swap if bx < cx.
  // Both have the same prec and same sign.
  // -------------------------------------------------------------------------

  let large: MPFR, small: MPFR;
  if (b.exp >= c.exp) {
    large = b; small = c;
  } else {
    large = c; small = b;
  }

  const bx = large.exp;
  const cx = small.exp;
  const d = bx - cx; // unsigned, >= 0

  // -------------------------------------------------------------------------
  // Compute the exact sum, choosing the bigint approach.
  //
  // Both operands have the same prec p = 128.
  //   S = large.mant * 2^d + small.mant   (exact positive bigint)
  //
  // S fits in at most p+1 bits (when bx==cx, sum always carries; when d>0,
  // only carries when a carry occurs in the high word).
  //
  // For d >= 2*p (= 256): small is completely swamped. The C code copies
  // large into ap and sets rb/sb directly, without adding small.
  // Ref: mpfr/src/add1sp.c L518-L532.
  //
  // For d == 128 (GMP_NUMB_BITS): rb=1, sb = (cp[0]!=0 || cp[1]>HIGHBIT).
  //   small.mant has 128 bits; cp[1] is the high 64 bits, cp[0] the low 64.
  //   In bigint: HIGHBIT_128 = 2^127.
  //   cp[0] = small.mant & LIMB_MASK; cp[1] = small.mant >> 64.
  //   cp[1] > HIGHBIT means cp[1] > 2^63, i.e. more than just the MSB is set.
  //   rb=1, sb = (cp[0] != 0n || cp[1] > LIMB_HIGHBIT).
  //
  // For d > 128: rb=0, sb=1 always.
  //
  // Ref: mpfr/src/add1sp.c L518-L573.
  // -------------------------------------------------------------------------

  const LIMB_HIGHBIT = 1n << 63n; // 2^63 = high bit of a 64-bit limb
  const LIMB_MASK = (1n << 64n) - 1n;
  const TWO_P = 2n * P; // 256n

  let mant: bigint;
  let exp: bigint;
  let ternary: Ternary;

  if (d >= TWO_P) {
    // Ref: mpfr/src/add1sp.c L518-L533 — d >= 2*GMP_NUMB_BITS branch.
    // Result significand is large.mant; just set round/sticky bits.
    const ap0 = large.mant & LIMB_MASK;          // low 64 bits of large
    const ap1 = large.mant >> 64n;               // high 64 bits of large

    let rb: bigint;
    let sb: bigint;

    if (d === TWO_P) {
      // d == 128 exactly: cp[0] + cp[1] contribute round+sticky.
      // Ref: mpfr/src/add1sp.c L520-L524.
      const cp0 = small.mant & LIMB_MASK;
      const cp1 = small.mant >> 64n;
      rb = 1n;
      sb = (cp0 !== 0n || cp1 > LIMB_HIGHBIT) ? 1n : 0n;
    } else {
      // d > 128: round bit = 0, sticky = 1.
      // Ref: mpfr/src/add1sp.c L526-L530.
      rb = 0n;
      sb = 1n;
    }

    // The output significand is large.mant; exp = bx (no carry).
    // Now perform rounding manually using rb/sb, mimicking C's rounding block.
    // Ref: mpfr/src/add1sp.c L576-L610.

    exp = bx;

    if (rb === 0n && sb === 0n) {
      // Exact.
      ternary = 0;
      mant = large.mant;
    } else {
      // Inexact: apply rounding.
      const result = roundWithRbSb(large.mant, P, exp, sign, rnd, rb, sb, ap0, ap1);
      mant = result.mant;
      exp = result.exp;
      ternary = result.ternary;
    }
  } else if (d === 0n) {
    // -------------------------------------------------------------------------
    // bx == cx case: both MSBs set, so a carry always occurs.
    // Ref: mpfr/src/add1sp.c L496-L506.
    //   a0 = bp[0] + cp[0]
    //   a1 = bp[1] + cp[1] + carry(a0)
    //   rb = a0 & 1  (before shift)
    //   sb = 0
    //   result = (a1, a0) >> 1, with MSB set in a1
    //   bx++
    // -------------------------------------------------------------------------

    const S = large.mant + small.mant; // p+1 bits (carry always)
    // S has exactly p+1 = 129 bits.
    // round bit = LSB of a0, but a0 = (S & LIMB_MASK before shifting).
    // Actually: the carry-and-shift produces a p-bit result; the round bit
    // is the LSB of the original sum (bit 0), and sticky=0.
    // In bigint: S >> 1 gives the p-bit significand; rb = S & 1n; sb = 0.
    const rb = S & 1n;
    const sb = 0n;
    const shifted = S >> 1n; // This has bit P set (carry out), now has P bits.

    exp = bx + 1n;

    if (rb === 0n && sb === 0n) {
      mant = shifted;
      ternary = 0;
    } else {
      const ap0 = shifted & LIMB_MASK;
      const ap1 = shifted >> 64n;
      const result = roundWithRbSb(shifted, P, exp, sign, rnd, rb, sb, ap0, ap1);
      mant = result.mant;
      exp = result.exp;
      ternary = result.ternary;
    }
  } else {
    // -------------------------------------------------------------------------
    // 0 < d < 2*GMP_NUMB_BITS case.
    // Ref: mpfr/src/add1sp.c L534-L573.
    // Compute S = large.mant + (small.mant >> d), with sticky bits from
    // the portion of small.mant that was shifted out.
    // -------------------------------------------------------------------------

    // The shifted-out portion of small is: small.mant & ((1n << d) - 1n).
    // This contributes the sb field in the C code.
    // After computing a0 and a1 (the two-limb sum), check if a carry occurs.
    // The C code computes sb first from bits shifted out of small, then:
    //   if carry in high word: rb = a0's MSB (before shift), shift right 1, bx++
    //   else:                  rb = MSB(sb), sb <<= 1

    const shiftedSmall = small.mant >> d;
    const lostBits = small.mant & ((1n << d) - 1n); // bits shifted out

    // Compute sum of large.mant and shiftedSmall.
    const S_approx = large.mant + shiftedSmall;
    const sLen = bitLen(S_approx);

    if (sLen > P) {
      // Carry occurred: result is S_approx >> 1, bx++.
      // Ref: mpfr/src/add1sp.c L557-L564.
      // rb is the LSB of S_approx (bit 0) before the shift — wait, no.
      // In C: rb = a0 << (GMP_NUMB_BITS - 1), which is bit GMP_NUMB_BITS-1
      // of the shifted result. Before the right-shift-by-1:
      //   original a0 has bit 0 = rb (after the right-shift by 1, this bit
      //   falls off the bottom of ap[0]).
      // Actually: after shift, ap[0] = (a1<<63)|(a0>>1), ap[1]=HIGHBIT|(a1>>1).
      // rb = the bit that lands just below the precision = a0 & 1 (before shift).
      // sb = lostBits (the bits originally shifted out of small).
      //
      // Wait — in C: rb = a0 << (GMP_NUMB_BITS-1), which means rb is testing
      // bit GMP_NUMB_BITS-1 of (a0 << (GMP_NUMB_BITS-1)), i.e. the high bit
      // of that expression = bit 0 of a0 BEFORE the right-shift. Yes: rb = a0 & 1.
      // sb = lostBits != 0 (bits shifted out from small).

      // Ref: mpfr/src/add1sp.c L559-L564.
      const rb = S_approx & 1n;
      // sb: the lostBits that were shifted out. sb is nonzero iff lostBits != 0.
      const sb = lostBits !== 0n ? 1n : 0n;
      const shifted = S_approx >> 1n;

      exp = bx + 1n;

      if (rb === 0n && sb === 0n) {
        mant = shifted;
        ternary = 0;
      } else {
        const ap0 = shifted & LIMB_MASK;
        const ap1 = shifted >> 64n;
        const result = roundWithRbSb(shifted, P, exp, sign, rnd, rb, sb, ap0, ap1);
        mant = result.mant;
        exp = result.exp;
        ternary = result.ternary;
      }
    } else {
      // No carry: result is S_approx, bx unchanged.
      // Ref: mpfr/src/add1sp.c L565-L572.
      // In C: rb = MPFR_LIMB_MSB(sb), sb <<= 1.
      // sb is the lostBits from small — but MPFR_LIMB_MSB is the MSB of the
      // C `sb` variable which is: cp[0] << (GMP_NUMB_BITS - d) [for d<64] or
      // (cp[1] << (2*GMP_NUMB_BITS-d)) | (cp[0]!=0) [for 64<=d<128].
      //
      // In bigint terms: the C sb variable encodes the shifted-out bits.
      // MSB(sb) = 1 iff the highest shifted-out bit is set = half-ulp.
      // After sb <<= 1, the remaining bits determine sticky.
      //
      // lostBits has d bits (positions 0..d-1 of small.mant).
      // The "round bit" (rb) in C corresponds to the topmost lost bit (bit d-1).
      // "sticky bit" (sb) corresponds to the remaining lost bits (bits 0..d-2).
      //
      // rb = (lostBits >> (d - 1n)) & 1n  (top lost bit)
      // sb = lostBits & ((1n << (d - 1n)) - 1n) != 0  (lower lost bits)

      const rb = (lostBits >> (d - 1n)) & 1n;
      const sb = (lostBits & ((1n << (d - 1n)) - 1n)) !== 0n ? 1n : 0n;

      exp = bx;

      if (rb === 0n && sb === 0n) {
        mant = S_approx;
        ternary = 0;
      } else {
        const ap0 = S_approx & LIMB_MASK;
        const ap1 = S_approx >> 64n;
        const result = roundWithRbSb(S_approx, P, exp, sign, rnd, rb, sb, ap0, ap1);
        mant = result.mant;
        exp = result.exp;
        ternary = result.ternary;
      }
    }
  }

  // -------------------------------------------------------------------------
  // Overflow check.
  // Ref: mpfr/src/add1sp.c L577-L578.
  // -------------------------------------------------------------------------

  if (exp > EMAX_DEFAULT) {
    return mpfr_overflow(P, rnd, sign);
  }

  const value: MPFR = {
    kind: 'normal',
    sign,
    prec: P,
    exp,
    mant,
  };
  return { value, ternary };
}

// ---------------------------------------------------------------------------
// Internal: rounding with explicit round/sticky bits.
// ---------------------------------------------------------------------------

/**
 * Perform rounding given a p-bit significand and explicit round/sticky bits.
 *
 * This mirrors the C rounding block at mpfr/src/add1sp.c L576-L610.
 *
 * The ap0/ap1 parameters are the low and high 64-bit limbs of `mant`
 * (used for the RNDN LSB tie-break check: `ap[0] & MPFR_LIMB_ONE`).
 *
 * Ref: mpfr/src/add1sp.c L581-L610 — the round/sticky rounding block.
 */
function roundWithRbSb(
  currentMant: bigint,
  p: bigint,
  currentExp: bigint,
  sign: Sign,
  rnd: RoundingMode,
  rb: bigint,
  sb: bigint,
  ap0: bigint,
  _ap1: bigint,
): { mant: bigint; exp: bigint; ternary: Ternary } {
  // rb and sb are 0n or 1n.
  // Ref: mpfr/src/add1sp.c L581-L588 — RNDN branch.
  // Ref: mpfr/src/add1sp.c L589-L594 — RNDZ branch (truncate).
  // Ref: mpfr/src/add1sp.c L595-L610 — add_one_ulp branch.

  // Determine whether to truncate or round up.
  let doTruncate: boolean;

  if (rnd === 'RNDN') {
    // Ref: mpfr/src/add1sp.c L583-L588:
    //   if (rb == 0 || (sb == 0 && (ap[0] & MPFR_LIMB_ONE) == 0)) → truncate
    //   else → add_one_ulp
    if (rb === 0n || (sb === 0n && (ap0 & 1n) === 0n)) {
      doTruncate = true;
    } else {
      doTruncate = false;
    }
  } else {
    // MPFR_IS_LIKE_RNDZ(rnd_mode, MPFR_IS_NEG(a)):
    // round toward zero with respect to sign.
    // Ref: mpfr/src/add1sp.c L589-L593.
    const isLikeRndz =
      rnd === 'RNDZ' ||
      (rnd === 'RNDD' && sign === 1) ||
      (rnd === 'RNDU' && sign === -1);
    doTruncate = isLikeRndz;
  }

  if (doTruncate) {
    // truncate: ternary = -MPFR_SIGN(a) = -(sign > 0 ? 1 : -1)
    // Ref: mpfr/src/add1sp.c L593: MPFR_RET(-MPFR_SIGN(a))
    // MPFR_RET(t) returns t * (MPFR_FLAGS set), so ternary = -sign for positive,
    // +sign for negative.  Wait: MPFR_RET(-MPFR_SIGN(a)):
    // if sign=1 (positive): returns -1 (rounded value < exact)  ✓
    // if sign=-1 (negative): returns +1 (rounded value > exact, less negative) ✓
    const ternary: Ternary = sign === 1 ? -1 : 1;
    return { mant: currentMant, exp: currentExp, ternary };
  }

  // add_one_ulp: ap[0] += 1, ap[1] += carry.
  // Ref: mpfr/src/add1sp.c L598-L609.
  const newMant = currentMant + 1n;
  const upperBound = 1n << p;

  if (newMant === upperBound) {
    // Carry out of the p-bit frame: set ap[1] = HIGHBIT, bx+1.
    // Ref: mpfr/src/add1sp.c L600-L607.
    // This is: ap[1] = MPFR_LIMB_HIGHBIT = 2^63; ap[0] = 0; bx++.
    // In bigint: the value is 2^(p-1) = 2^127, exp++.
    const newExp = currentExp + 1n;
    // Check overflow again (handled by caller, but guard here too).
    if (newExp > EMAX_DEFAULT) {
      // Will be caught by the caller; return with incremented exp.
      return {
        mant: 1n << (p - 1n),
        exp: newExp,
        ternary: sign === 1 ? 1 : -1,
      };
    }
    return {
      mant: 1n << (p - 1n),
      exp: newExp,
      ternary: sign === 1 ? 1 : -1,
    };
  }

  // Ref: mpfr/src/add1sp.c L609: MPFR_RET(MPFR_SIGN(a)).
  // ternary = +sign for positive (rounded up > exact), -sign for negative.
  const ternary: Ternary = sign === 1 ? 1 : -1;
  return { mant: newMant, exp: currentExp, ternary };
}
