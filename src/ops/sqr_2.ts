/**
 * ops/sqr_2.ts — pure-TS port of MPFR's `mpfr_sqr_2`.
 *
 * Two-limb squaring fast path for `GMP_NUMB_BITS < p < 2 * GMP_NUMB_BITS`,
 * i.e. 65 <= p <= 127 on x86_64 with prec(a) == prec(b).
 *
 * C signature
 * -----------
 *
 *   static int mpfr_sqr_2(mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode,
 *                          mpfr_prec_t p)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_sqr_2(b, prec, rnd) -> Result
 *
 * The function is 'static' in C; graded via public mpfr_sqr with prec(b)
 * in (64, 128) and prec(a) == prec(b).
 *
 * Algorithm
 * ---------
 *
 * Ref: mpfr/src/sqr.c L232–L356 — the full C reference body.
 *
 * The C code computes:
 *   1. umul_ppmm(h, l, bp[1], bp[1])  — high square of top limb
 *   2. umul_ppmm(u, v, bp[1], bp[0])  — cross product
 *   3. l += u << 1; h += carry + (u >> 63)  — accumulate cross product * 2
 *   4. Early-exit approximation: if (l+2) & (mask >> 2) > 2, sb = sb2 = 1
 *   5. Otherwise: full b0^2 via umul_ppmm(sb, sb2, bp[0], bp[0]);
 *      add in 2*v to the {h,l,sb} using ADD_LIMB twice.
 *   6. If h < HIGHBIT: shift left by 1 (h, l, sb) and decrement ax.
 *   7. Extract rb, sb, ap[0], ap[1].
 *   8. Rounding and overflow/underflow checks.
 *
 * TS approach
 * -----------
 *
 * Instead of simulating four 64-bit limbs, we use BigInt multiplication
 * directly. The mantissa `b.mant` is a `p`-bit integer (MSB-aligned).
 *
 * The 128-bit C-limb form is:
 *   climbMant = b.mant << sh   (where sh = 128 - p, sh in [1, 63])
 *   bp1 = climbMant >> 64   (high 64 bits)
 *   bp0 = climbMant & MASK64  (low 64 bits)
 *
 * The exact square of b in C-limb form: (bp1 * 2^64 + bp0)^2.
 * This is a 256-bit (4-limb) value.
 *
 * The C algorithm computes only the top 128 bits + enough info for
 * rounding. We mirror that exactly by:
 *   1. Computing climbMant^2 as a 256-bit BigInt.
 *   2. Tracking the approximation vs. exact paths.
 *   3. Extracting h (bits [255:192]), l (bits [191:128]),
 *      sb/sb2 from the lower 128 bits.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sqr.c L232–L356 — the C reference body.
 *   - mpfr/src/sqr.c L545–L546 — dispatcher.
 *   - mpfr/src/mpfr-impl.h L1762 — ADD_LIMB(u,v,c): (u += v, c = (u < v)).
 *   - mpfr/src/mpfr-impl.h L1233–1236 — MPFR_IS_LIKE_RNDZ / MPFR_IS_LIKE_RNDA.
 *   - CLAUDE.md §"Hallucination-risk callouts" — umul_ppmm output args are first.
 *   - CLAUDE.md §"Ternary flag is the sign of (rounded - exact), not 0/1."
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from "../core.ts";
import { MPFRError, PREC_MIN, PREC_MAX } from "../core.ts";
import { mpfr_overflow } from "./overflow.ts";
import { mpfr_underflow } from "./underflow.ts";

// ---------------------------------------------------------------------------
// Constants
// Ref: mpfr/src/mpfr-impl.h L1300-L1311
// ---------------------------------------------------------------------------

/** GMP_NUMB_BITS = 64 on x86_64. */
const GMP_NUMB_BITS = 64n;

/** 64-bit mask: all 64 bits set. */
const MASK64 = (1n << GMP_NUMB_BITS) - 1n;

/** MPFR_LIMB_HIGHBIT = 1 << 63. Ref: mpfr/src/mpfr-impl.h L1301. */
const LIMB_HIGHBIT = 1n << 63n;

/** MPFR_LIMB_MAX = all 64 bits set = MASK64. */
const LIMB_MAX = MASK64;

/**
 * Default exponent ceiling — matches `__gmpfr_emax` on fresh init.
 * Ref: mpfr/src/mpfr.h L231 — `MPFR_EMAX_DEFAULT = (1 << 30) - 1`.
 */
const EMAX_DEFAULT = (1n << 30n) - 1n;

/**
 * Default exponent floor — matches `__gmpfr_emin` on fresh init.
 * Ref: mpfr/src/mpfr.h L228 — `MPFR_EMIN_DEFAULT = (1 - (1 << 30))`.
 */
const EMIN_DEFAULT = 1n - (1n << 30n);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ (mpfr-impl.h L1233-L1234):
 * For a POSITIVE result (sign=+1), MPFR_IS_LIKE_RNDZ(rnd, 0):
 *   rnd == RNDZ  ||  (rnd == RNDD && !0)  =  RNDZ || RNDD.
 * Here `neg` is always 0 because sqr always produces a positive result.
 * Ref: mpfr/src/sqr.c L334 — MPFR_IS_LIKE_RNDZ(rnd_mode, 0).
 */
function isLikeRNDZ_pos(rnd: RoundingMode): boolean {
  return rnd === 'RNDZ' || rnd === 'RNDD';
}

/**
 * TS analogue of MPFR_IS_LIKE_RNDA (mpfr-impl.h L1235-L1236):
 * For a POSITIVE result, MPFR_IS_LIKE_RNDA(rnd, 0):
 *   rnd == RNDA  ||  (RNDZ/RNDD/RNDU logic with neg=0 flipped)
 *   = rnd == RNDA || RNDU
 * Actually: MPFR_IS_LIKE_RNDA(rnd, neg) = (rnd==RNDA) || MPFR_IS_RNDUTEST_OR_RNDDNOTTEST(rnd, neg==0)
 * MPFR_IS_RNDUTEST_OR_RNDDNOTTEST(rnd, cond) = (cond ? rnd==RNDU : rnd==RNDD)
 * With neg=0 and cond=(neg==0)=1: = (rnd==RNDU)
 * So MPFR_IS_LIKE_RNDA(rnd, 0) = (rnd==RNDA) || (rnd==RNDU).
 * Ref: mpfr/src/sqr.c L308 — MPFR_IS_LIKE_RNDA(rnd_mode, 0).
 */
function isLikeRNDA_pos(rnd: RoundingMode): boolean {
  return rnd === 'RNDA' || rnd === 'RNDU';
}

// ---------------------------------------------------------------------------
// Port
// ---------------------------------------------------------------------------

/**
 * Square `b` at precision `prec` with rounding mode `rnd`.
 *
 * @mpfrName mpfr_sqr_2 (static)
 *
 * @param b    Operand; must be `kind='normal'`, `65 <= b.prec <= 127`.
 * @param prec Target precision in bits, `65 <= prec <= 127`, must equal `b.prec`.
 * @param rnd  Rounding mode.
 *
 * @returns `{ value, ternary }`.
 *
 * @throws {MPFRError} `EPREC` on invalid `prec`; `EROUND` on invalid `rnd`.
 */
export function mpfr_sqr_2(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate inputs
  // Ref: mpfr/src/sqr.c L545 — dispatcher guarantees GMP_NUMB_BITS < aq < 2*GMP_NUMB_BITS
  // -------------------------------------------------------------------------
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_sqr_2: prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_sqr_2: prec out of range: ${prec}`);
  }
  if (prec <= GMP_NUMB_BITS || prec >= 2n * GMP_NUMB_BITS) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sqr_2: prec must be in (64, 128), got ${prec}`,
    );
  }

  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_sqr_2: unknown rounding mode '${String(rnd)}'`);
  }

  // Handle special b values (NaN, Inf, Zero).
  // Ref: mpfr/src/sqr.c L521–L535 — singular handling in the public mpfr_sqr.
  if (b.kind === 'nan') {
    return { value: { kind: 'nan', sign: 1, prec: 0n, exp: 0n, mant: 0n }, ternary: 0 };
  }
  if (b.kind === 'inf') {
    // sqr(+/-inf) = +inf
    return { value: { kind: 'inf', sign: 1, prec, exp: 0n, mant: 0n }, ternary: 0 };
  }
  if (b.kind === 'zero') {
    // sqr(+/-0) = +0
    return { value: { kind: 'zero', sign: 1, prec, exp: 0n, mant: 0n }, ternary: 0 };
  }

  // -------------------------------------------------------------------------
  // b is 'normal'. Squaring always produces a positive result.
  // Ref: mpfr/src/sqr.c L287 — MPFR_SIGN(a) = MPFR_SIGN_POS.
  // -------------------------------------------------------------------------
  const p = prec;  // target precision (same as b.prec per the dispatcher)

  // sh = 2 * GMP_NUMB_BITS - p = 128 - p, in [1, 63].
  // This is the number of "spare" low bits in the 128-bit C-limb form.
  // Ref: mpfr/src/sqr.c L238.
  const sh = 2n * GMP_NUMB_BITS - p;  // sh in [1, 63]

  // mask = MPFR_LIMB_MASK(sh) = (1 << sh) - 1.
  // Ref: mpfr/src/mpfr-impl.h — MPFR_LIMB_MASK macro.
  const mask = (1n << sh) - 1n;

  // ax = 2 * b.exp  (exponent of the square before normalization)
  // Ref: mpfr/src/sqr.c L237.
  let ax = 2n * b.exp;

  // -------------------------------------------------------------------------
  // Convert b.mant (p-bit, MSB-aligned) to C 2-limb form (128-bit MSB-aligned).
  // climbMant = b.mant << sh  (right-pads to 128 bits)
  // bp1 = high 64 bits, bp0 = low 64 bits.
  // Ref: spec — "converting TS-schema mantissas to C-limb form".
  // -------------------------------------------------------------------------
  const climbMant = b.mant << sh;
  const bp1 = climbMant >> GMP_NUMB_BITS;  // high 64 bits of the 128-bit C form
  const bp0 = climbMant & MASK64;          // low 64 bits

  // -------------------------------------------------------------------------
  // Step 1: compute (bp1)^2 via umul_ppmm(h, l, bp1, bp1).
  // umul_ppmm writes: h = high 64 bits, l = low 64 bits of bp1 * bp1.
  // Ref: mpfr/src/sqr.c L243 — umul_ppmm (h, l, bp[1], bp[1]).
  // CLAUDE.md callout: umul_ppmm output args are first.
  // -------------------------------------------------------------------------
  const sq1 = bp1 * bp1;  // 128-bit product
  let h = sq1 >> GMP_NUMB_BITS;   // high 64 bits
  let l = sq1 & MASK64;           // low 64 bits

  // Step 2: compute cross product umul_ppmm(u, v, bp1, bp0).
  // Ref: mpfr/src/sqr.c L244 — umul_ppmm (u, v, bp[1], bp[0]).
  const cross = bp1 * bp0;
  const u = cross >> GMP_NUMB_BITS;  // high 64 bits
  const v = cross & MASK64;          // low 64 bits

  // Step 3: accumulate: l += u << 1; h += (l < u<<1) + (u >> 63).
  // This adds 2 * cross into the upper 128 bits.
  // Ref: mpfr/src/sqr.c L245–L246.
  const u_shl1 = (u << 1n) & MASK64;  // u << 1, truncated to 64 bits (carry handled below)
  const u_shl1_carry = u >> 63n;  // carry from u << 1 (top bit of u shifted out)
  const l_before = l;
  l = (l + u_shl1) & MASK64;
  // l_carry is 1 if l_before + u_shl1 overflowed 64 bits
  const l_overflow = (l_before + u_shl1) >> GMP_NUMB_BITS;  // 0 or 1
  h = (h + l_overflow + u_shl1_carry) & MASK64;
  // Note: h can exceed 64 bits if it was near LIMB_MAX, but for valid MPFR mantissas
  // (MSB set, so bp1 >= HIGHBIT), h >= 1/4 * 2^128 / 2^64 = HIGHBIT, so h has MSB set
  // most of the time. We keep h & MASK64.

  // -------------------------------------------------------------------------
  // The full 256-bit product is: {h, l, 2*v + high(bp0^2), low(bp0^2)}
  // where h and l are our high 128 bits.
  //
  // Step 4: Early approximation check.
  // "if (((l + 2) & (mask >> 2)) > 2) then sb = sb2 = 1"
  // This says: if the low bits of l (after adding 2) are not all-0/all-1/all-2,
  // we can determine the rounding without computing the full lower 128 bits.
  // Ref: mpfr/src/sqr.c L259–L260.
  // -------------------------------------------------------------------------
  let sb: bigint;
  let sb2: bigint;

  const mask_rsh2 = mask >> 2n;
  if (mask_rsh2 > 0n && (((l + 2n) & mask_rsh2) > 2n)) {
    // Fast path: can't be exact, sticky bits are nonzero.
    sb = 1n;
    sb2 = 1n;
  } else {
    // Slow path: compute b0^2 and add in the full lower contribution.
    // umul_ppmm(sb, sb2, bp0, bp0):
    // Ref: mpfr/src/sqr.c L265 — umul_ppmm (sb, sb2, bp[0], bp[0]).
    const sq0 = bp0 * bp0;
    sb = sq0 >> GMP_NUMB_BITS;   // high 64 bits of bp0^2
    sb2 = sq0 & MASK64;          // low 64 bits of bp0^2

    // ADD_LIMB (sb, v, carry1): sb += v; carry1 = (sb < v) [overflow]
    // Ref: mpfr/src/sqr.c L267 — first ADD_LIMB(sb, v, carry1).
    // mpfr/src/mpfr-impl.h L1762 — #define ADD_LIMB(u,v,c) ((u)+=(v), (c)=(u)<(v))
    const sb_v1 = (sb + v) & MASK64;
    const carry1a = ((sb + v) >> GMP_NUMB_BITS) > 0n ? 1n : 0n;
    sb = sb_v1;

    // ADD_LIMB (l, carry1, carry2): l += carry1; carry2 = (l < carry1)
    // Ref: mpfr/src/sqr.c L268.
    const l_c1 = (l + carry1a) & MASK64;
    const carry2a = ((l + carry1a) >> GMP_NUMB_BITS) > 0n ? 1n : 0n;
    l = l_c1;
    h = (h + carry2a) & MASK64;

    // Second time: add v again (the full product includes 2*v)
    // ADD_LIMB (sb, v, carry1): sb += v; carry1 = (sb < v)
    // Ref: mpfr/src/sqr.c L270.
    const sb_v2 = (sb + v) & MASK64;
    const carry1b = ((sb + v) >> GMP_NUMB_BITS) > 0n ? 1n : 0n;
    sb = sb_v2;

    // ADD_LIMB (l, carry1, carry2): l += carry1; carry2 = (l < carry1)
    // Ref: mpfr/src/sqr.c L271.
    const l_c1b = (l + carry1b) & MASK64;
    const carry2b = ((l + carry1b) >> GMP_NUMB_BITS) > 0n ? 1n : 0n;
    l = l_c1b;
    h = (h + carry2b) & MASK64;
  }

  // -------------------------------------------------------------------------
  // Step 5: Normalize.
  // If h < HIGHBIT, shift {h, l, sb} left by 1 and decrement ax.
  // Ref: mpfr/src/sqr.c L274–L281.
  // -------------------------------------------------------------------------
  if (h < LIMB_HIGHBIT) {
    ax -= 1n;
    h = ((h << 1n) | (l >> 63n)) & MASK64;
    l = ((l << 1n) | (sb >> 63n)) & MASK64;
    sb = (sb << 1n) & MASK64;
    // sb2 is not shifted since we only need to know if it's zero or not.
    // Ref: mpfr/src/sqr.c L280 — "no need to shift sb2 since we only want to know if it is zero or not".
  }

  // -------------------------------------------------------------------------
  // Step 6: Extract results.
  // ap[1] = h, rb = l & (1 << (sh-1)), sb |= ((l & mask) ^ rb) | sb2, ap[0] = l & ~mask.
  // Ref: mpfr/src/sqr.c L282–L285.
  // -------------------------------------------------------------------------
  const ap1 = h;
  const rb = l & (1n << (sh - 1n));
  sb = (sb | ((l & mask) ^ rb) | sb2) & MASK64;
  const ap0 = l & ~mask;

  // Result is always positive (squaring).
  // Ref: mpfr/src/sqr.c L287 — MPFR_SIGN(a) = MPFR_SIGN_POS.
  const resultSign: Sign = 1;

  // -------------------------------------------------------------------------
  // Step 7: Overflow check.
  // Ref: mpfr/src/sqr.c L290 — if (ax > __gmpfr_emax) return mpfr_overflow(...).
  // -------------------------------------------------------------------------
  if (ax > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, resultSign);
  }

  // -------------------------------------------------------------------------
  // Step 8: Underflow check.
  // Ref: mpfr/src/sqr.c L296–L317.
  // -------------------------------------------------------------------------
  if (ax < EMIN_DEFAULT) {
    // Check if we can avoid underflow via rounding.
    // Ref: mpfr/src/sqr.c L304–L309.
    const ap1_is_max = ap1 === LIMB_MAX;
    const ap0_eq_nmask = ap0 === (~mask & MASK64);
    if (
      ax === EMIN_DEFAULT - 1n &&
      ap1_is_max &&
      ap0_eq_nmask &&
      ((rnd === 'RNDN' && rb !== 0n) ||
       (isLikeRNDA_pos(rnd) && (rb | sb) !== 0n))
    ) {
      // No underflow — goto rounding.
      // Fall through to rounding below.
    } else {
      // Actual underflow.
      // For RNDN, if ax < emin-1 or result is a power of two with no rounding bits,
      // use RNDZ instead.
      // Ref: mpfr/src/sqr.c L312–L315.
      let rndEff: RoundingMode = rnd;
      if (rnd === 'RNDN') {
        if (
          ax < EMIN_DEFAULT - 1n ||
          (ap1 === LIMB_HIGHBIT && ap0 === 0n && (rb | sb) === 0n)
        ) {
          rndEff = 'RNDZ';
        }
      }
      return mpfr_underflow(p, rndEff, resultSign);
    }
  }

  // -------------------------------------------------------------------------
  // Step 9: Rounding.
  // MPFR_EXP(a) = ax.
  // Ref: mpfr/src/sqr.c L319–L355.
  //
  // Ternary: sign of (rounded - exact).
  //   exact -> ternary = 0
  //   truncate -> ternary = -1 (positive result rounded down)  = -MPFR_SIGN_POS
  //   add_one_ulp -> ternary = +1 (positive result rounded up)  = +MPFR_SIGN_POS
  // Ref: mpfr/src/sqr.c L338 — MPFR_RET(-MPFR_SIGN_POS) = -1.
  //      mpfr/src/sqr.c L354 — MPFR_RET(MPFR_SIGN_POS) = +1.
  // -------------------------------------------------------------------------

  // Helper: build result MPFR from 2-limb C form at (ap1, ap0).
  // TS mant = (ap1 * 2^64 + ap0) >> sh.
  function buildResult(bx: bigint, a1: bigint, a0: bigint): MPFR {
    const full = (a1 << GMP_NUMB_BITS) | a0;
    const mant = full >> sh;
    return {
      kind: 'normal',
      sign: resultSign,
      prec: p,
      exp: bx,
      mant,
    } satisfies MPFR;
  }

  if (rb === 0n && sb === 0n) {
    // Exact.
    // Ref: mpfr/src/sqr.c L322 — if (rb == 0 && sb == 0) MPFR_RET(0).
    return { value: buildResult(ax, ap1, ap0), ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // Ref: mpfr/src/sqr.c L327–L332.
    // Truncate if rb == 0, or (sb == 0 and LSB of result (ap[0] bit sh) == 0).
    const lsbBit = ap0 & (1n << sh);
    if (rb === 0n || (sb === 0n && lsbBit === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ_pos(rnd)) {
    // Round toward zero (for positive result: RNDZ or RNDD).
    // Ref: mpfr/src/sqr.c L334 — MPFR_IS_LIKE_RNDZ(rnd_mode, 0).
    doAddOneUlp = false;
  } else {
    // Round away from zero (for positive result: RNDU or RNDA).
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // Truncate: ternary = -1 (rounded down from positive exact value).
    // Ref: mpfr/src/sqr.c L338 — MPFR_RET(-MPFR_SIGN_POS) = -1.
    const ternary: Ternary = -1;
    return { value: buildResult(ax, ap1, ap0), ternary };
  }

  // add_one_ulp:
  // ap[0] += 1 << sh; ap[1] += (ap[0] == 0); if ap[1] == 0: overflow.
  // Ref: mpfr/src/sqr.c L343–L355.
  const newAp0 = (ap0 + (1n << sh)) & MASK64;
  let newAp1 = (ap1 + (newAp0 === 0n ? 1n : 0n)) & MASK64;
  let newAx = ax;

  if (newAp1 === 0n) {
    // ap[1] wrapped to 0 — mantissa overflowed.
    // ap[1] = HIGHBIT; ax += 1; check emax.
    // Ref: mpfr/src/sqr.c L347–L353.
    newAp1 = LIMB_HIGHBIT;
    newAx = ax + 1n;
    if (newAx > EMAX_DEFAULT) {
      return mpfr_overflow(p, rnd, resultSign);
    }
  }

  const ternary: Ternary = 1;  // rounded up
  return { value: buildResult(newAx, newAp1, newAp0), ternary };
}
