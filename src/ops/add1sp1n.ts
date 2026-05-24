/**
 * ops/add1sp1n.ts — pure-TS port of MPFR's `mpfr_add1sp1n`.
 *
 * Same-precision same-sign addition fast path for `p == GMP_NUMB_BITS == 64`.
 * This is the 64-bit single-limb sibling of `mpfr_add1sp1` (which handles
 * `p < 64`). When `sh == 0`, mantissas are already MSB-aligned to 64 bits —
 * the C-limb form and the TS-schema `mant` are identical. No shifting in or
 * out is needed; the algebra is therefore simpler and faster than the `p < 64`
 * version, but the sub-cases change slightly.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_add1sp1n(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                             mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_add1sp1n(b, c, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS)
 * ----------------------------------------------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.sign === c.sign` (same-sign addition; caller ensures this)
 *   - `b.prec === c.prec === 64n`
 *
 * Algorithm (sh == 0)
 * -------------------
 *
 * With sh = GMP_NUMB_BITS - p = 64 - 64 = 0, the mantissa already lives
 * in 64-bit limb form.
 *
 *   Case A (bx == cx):
 *     a0 = (bp0 + cp0) mod 2^64   -- exact sum overflows since both MSBs set
 *     rb = a0 & 1                  -- MPFR_LIMB_ONE (bit 0)
 *     ap0 = HIGHBIT | (a0 >> 1)   -- halved back to [HIGHBIT, MAX64)
 *     bx++
 *     sb = 0
 *
 *   Case B1 (1 <= d < 64, d = bx - cx):
 *     a0 = (bp0 + (cp0 >> d)) mod 2^64
 *     sb = (cp0 << (64-d)) mod 2^64    -- bits that fell below bit 0
 *     if carry (a0 < bp0 in 64-bit arithmetic):
 *       ap0 = HIGHBIT | (a0 >> 1)
 *       rb = a0 & 1
 *       bx++
 *     else:
 *       ap0 = a0
 *       rb = sb & HIGHBIT
 *       sb &= ~HIGHBIT
 *
 *   Case B2 (d >= 64):
 *     sb = (d != 64 || cp0 != HIGHBIT) ? 1 : 0
 *     ap0 = bp0
 *     rb = (d == 64n) ? 1 : 0
 *
 * Rounding proceeds with MPFR_LIMB_ONE = 1 (not 1 << sh, since sh == 0):
 *   - add_one_ulp: ap0 += 1; if ap0 == 0 → ap0 = HIGHBIT, bx++, overflow?
 *   - RNDN tie check: `ap0 & 1` (LSB of the 64-bit mantissa)
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add1sp.c L256-L358 — verbatim C reference body.
 *   - mpfr/src/add1sp.c L1078-L1080 — dispatcher routing.
 *   - mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ.
 *   - mpfr/src/mpfr-impl.h L1300-L1311 — MPFR_LIMB_HIGHBIT, MPFR_LIMB_ONE.
 *   - mpfr/src/mpfr.h L231 — MPFR_EMAX_DEFAULT = (1 << 30) - 1.
 *   - src/ops/add1sp1.ts — sister op for p < 64 (sh > 0).
 *   - src/ops/overflow.ts — delegate for exponent overflow.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign
 *     of (rounded - exact), not 0/1."
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../src/core.ts';
import { MPFRError } from '../../src/core.ts';
import { mpfr_overflow } from '../../src/ops/overflow.ts';

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
// Helper
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

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign addition fast path for `p == 64`.
 *
 * Mirrors `mpfr_add1sp1n` from `mpfr/src/add1sp.c L256-L358`.
 *
 * @mpfrName mpfr_add1sp1n
 *
 * @param b   First addend; must be `kind='normal'`, `prec === 64n`.
 * @param c   Second addend; must be `kind='normal'`, same `prec` and `sign`.
 * @param rnd Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }` — the correctly-rounded sum and the
 *          ternary flag (sign of rounded − exact).
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_add1sp1n(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions.
  // Ref: mpfr/src/add1sp.c L270-L272 — MPFR_ASSERTD checks.
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp1n: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp1n: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== 64n) {
    throw new MPFRError('EPREC', `mpfr_add1sp1n: b.prec must be 64n, got ${b.prec}`);
  }
  if (c.prec !== 64n) {
    throw new MPFRError('EPREC', `mpfr_add1sp1n: c.prec must be 64n, got ${c.prec}`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_add1sp1n: b.sign (${b.sign}) !== c.sign (${c.sign})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_add1sp1n: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Set up C-mirror variables.
  // Ref: mpfr/src/add1sp.c L260-L268.
  //
  // Since sh = 64 - 64 = 0, the TS-schema mant (MSB-aligned to prec=64 bits)
  // is already the C-limb form. No shift in or out needed.
  // -------------------------------------------------------------------------

  const sign = b.sign;    // same for both operands

  // bp0, cp0 are the 64-bit C limbs — identical to b.mant and c.mant for p=64.
  let bp0: bigint = b.mant;
  let cp0: bigint = c.mant;

  let bx: bigint = b.exp;
  const cx_orig: bigint = c.exp;
  // We will use cx as the potentially-swapped exponent:
  let cx: bigint = cx_orig;

  // -------------------------------------------------------------------------
  // Core arithmetic: three cases.
  // Ref: mpfr/src/add1sp.c L274-L316.
  // -------------------------------------------------------------------------

  let ap0: bigint;  // the result 64-bit limb
  let rb: bigint;   // round bit (nonzero <=> round bit is set)
  let sb: bigint;   // sticky bit (nonzero <=> any lost bit was set)

  if (bx === cx) {
    // -----------------------------------------------------------------------
    // Case A: equal exponents.
    // Ref: mpfr/src/add1sp.c L274-L281.
    //
    // Both operands have MSB set (they are normal), so bp0 + cp0 always
    // overflows a 64-bit limb.  The exact sum occupies 65 bits; we shift
    // right by 1 and bump the exponent.
    //
    //   a0 = (bp0 + cp0) mod 2^64   (64-bit overflow is guaranteed)
    //   rb = a0 & MPFR_LIMB_ONE     (= a0 & 1, the bit shifted off)
    //   ap0 = HIGHBIT | (a0 >> 1)   (restore MSB, halve)
    //   bx++
    //   sb = 0
    // -----------------------------------------------------------------------
    const a0 = (bp0 + cp0) & LIMB_MASK_64;  // 64-bit wrapping add
    rb = a0 & 1n;                             // MPFR_LIMB_ONE = 1 (sh == 0)
    ap0 = LIMB_HIGHBIT | (a0 >> 1n);
    bx = bx + 1n;
    sb = 0n;
  } else {
    // Ensure bx >= cx (swap if needed).
    // Ref: mpfr/src/add1sp.c L284-L290.
    if (bx < cx) {
      const tx = bx; bx = cx; cx = tx;
      const tp = bp0; bp0 = cp0; cp0 = tp;
    }
    // Now bx > cx.  d = bx - cx > 0.
    const d: bigint = bx - cx;  // mpfr_uexp_t: always positive

    if (d < GMP_NUMB_BITS) {
      // ---------------------------------------------------------------------
      // Case B1: 1 <= d < 64.
      // Ref: mpfr/src/add1sp.c L293-L309.
      //
      // a0 = bp0 + (cp0 >> d)  (may overflow 64 bits — carry detected by a0 < bp0)
      // sb = cp0 << (64 - d)   (the bits from cp that fall below the result window)
      // ---------------------------------------------------------------------
      const a0_full = (bp0 + (cp0 >> d)) & LIMB_MASK_64;
      const sb_raw = (cp0 << (GMP_NUMB_BITS - d)) & LIMB_MASK_64;

      // Carry detection: in 64-bit unsigned arithmetic, a0 < bp0 iff there was a carry.
      // Ref: mpfr/src/add1sp.c L297 — `if (a0 < bp[0])`.
      if (a0_full < bp0) {
        // carry branch
        ap0 = LIMB_HIGHBIT | (a0_full >> 1n);
        rb = a0_full & 1n;   // MPFR_LIMB_ONE = 1
        bx = bx + 1n;
        sb = sb_raw;         // sb stays as-is (no contribution to shift-down)
      } else {
        // no-carry branch
        ap0 = a0_full;
        rb = sb_raw & LIMB_HIGHBIT;      // top bit of sb becomes round bit
        sb = sb_raw & ~LIMB_HIGHBIT;     // remaining sb bits (mask off HIGHBIT)
      }
    } else {
      // ---------------------------------------------------------------------
      // Case B2: d >= 64.
      // Ref: mpfr/src/add1sp.c L310-L315.
      //
      // cp is so much smaller than bp that it contributes only sticky/round.
      //   sb = (d != 64 || cp0 != HIGHBIT) ? 1 : 0
      //   ap0 = bp0
      //   rb = (d == 64n) ? 1 : 0
      // ---------------------------------------------------------------------
      ap0 = bp0;
      rb = (d === GMP_NUMB_BITS) ? 1n : 0n;
      sb = (d !== GMP_NUMB_BITS || cp0 !== LIMB_HIGHBIT) ? 1n : 0n;
    }
  }

  // -------------------------------------------------------------------------
  // Overflow check (pre-rounding).
  // Ref: mpfr/src/add1sp.c L323-L324.
  // -------------------------------------------------------------------------

  if (bx > EMAX_DEFAULT) {
    return mpfr_overflow(64n, rnd, sign);
  }

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: mpfr/src/add1sp.c L327-L357.
  //
  // Ternary convention: sign of (rounded − exact).
  //   truncate → rounded < exact → ternary = −sign
  //     (C: MPFR_RET(-MPFR_SIGN(a))  where MPFR_SIGN(a) > 0 for positive)
  //   add_one_ulp → rounded > exact → ternary = +sign
  //     (C: MPFR_RET(MPFR_SIGN(a)))
  //
  // For positive sign (+1): truncate → ternary = -1; add_one_ulp → ternary = +1.
  // For negative sign (-1): truncate → ternary = +1; add_one_ulp → ternary = -1.
  //
  // RNDN tie-to-even check: `ap0 & MPFR_LIMB_ONE` = `ap0 & 1` (since sh == 0).
  // Ref: mpfr/src/add1sp.c L333 — `(ap[0] & MPFR_LIMB_ONE) == 0`.
  // -------------------------------------------------------------------------

  if (rb === 0n && sb === 0n) {
    // Exact result.
    // Ref: mpfr/src/add1sp.c L327 — `if ((rb == 0 && sb == 0) ...) MPFR_RET(0)`.
    return { value: buildResult(sign, bx, ap0), ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // Round to nearest, ties to even.
    // Ref: mpfr/src/add1sp.c L329-L337.
    //
    // Truncate if rb == 0 (below midpoint), or if rb != 0, sb == 0, and
    // the last stored bit (ap0 & 1) is 0 (tie goes to even = 0 → truncate).
    const lsb = ap0 & 1n;  // MPFR_LIMB_ONE = 1 since sh == 0
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    // Truncate toward zero.
    // Ref: mpfr/src/add1sp.c L338-L341 — `goto truncate`.
    doAddOneUlp = false;
  } else {
    // Round away from zero (RNDA, or RNDU/RNDD in the away-from-zero direction).
    // Ref: mpfr/src/add1sp.c L342-L357 — `goto add_one_ulp`.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // truncate branch: ternary = -sign.
    // Ref: mpfr/src/add1sp.c L341 — `MPFR_RET(-MPFR_SIGN(a))`.
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value: buildResult(sign, bx, ap0), ternary };
  }

  // add_one_ulp branch.
  // Ref: mpfr/src/add1sp.c L346-L356.
  //
  // ap[0] += MPFR_LIMB_ONE (= 1 since sh == 0).
  // If ap[0] wraps to 0 (all-bits-set → carry):
  //   ap[0] = HIGHBIT, bx++, check overflow.

  let newAp0 = (ap0 + 1n) & LIMB_MASK_64;
  let newBx = bx;

  if (newAp0 === 0n) {
    // Mantissa carry-out: the add_one_ulp rippled all the way through.
    // Ref: mpfr/src/add1sp.c L347-L354.
    newAp0 = LIMB_HIGHBIT;
    newBx = bx + 1n;
    if (newBx > EMAX_DEFAULT) {
      return mpfr_overflow(64n, rnd, sign);
    }
  }

  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  return { value: buildResult(sign, newBx, newAp0), ternary };
}

// ---------------------------------------------------------------------------
// buildResult: construct TS-schema MPFR from the 64-bit limb result.
// ---------------------------------------------------------------------------

/**
 * Construct the TS-schema `MPFR` normal value from C-limb arithmetic results.
 *
 * For `p == 64` (sh == 0), the mantissa in TS schema equals the 64-bit limb
 * directly — no right-shift conversion needed.
 *
 * Invariant: `ap0` must satisfy `HIGHBIT <= ap0 < 2^64` (MSB set) so that
 * the MPFR normal MSB-normalisation requirement (`mant >= 2^(prec-1)`) holds.
 *
 * Ref: mpfr/src/add1sp.c L326 — `MPFR_SET_EXP(a, bx)` followed by storing
 *   `ap[0]` which is the normalised 64-bit result limb.
 */
function buildResult(sign: Sign, exp: bigint, ap0: bigint): MPFR {
  return {
    kind: 'normal',
    sign,
    prec: 64n,
    exp,
    mant: ap0,
  } satisfies MPFR;
}
