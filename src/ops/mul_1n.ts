/**
 * ops/mul_1n.ts — pure-TS port of MPFR's `mpfr_mul_1n`.
 *
 * Single-limb multiplication fast path for `prec(a) == GMP_NUMB_BITS == 64`
 * and `prec(b), prec(c) <= 64`. The 'n' suffix means "native single limb"
 * (no `sh` shift — the result mantissa fills the 64-bit limb exactly).
 *
 * This differs from `mpfr_mul_1` (which handles `p < 64`): here `sh == 0`,
 * so the output limb `a0` occupies all 64 bits and no masking of a trailing
 * `sh` bits is needed. The round bit (`rb`) is the MSB of the low product
 * limb and the sticky bits (`sb`) are the remaining bits of that low limb.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_mul_1n(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                           mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_mul_1n(b, c, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS)
 * ----------------------------------------------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.prec <= 64n` and `c.prec <= 64n`
 *   - Output precision is implicitly 64 bits (the 'n' variant)
 *
 * Algorithm
 * ---------
 *
 * 1. `ax = b.exp + c.exp`  — provisional result exponent.
 * 2. `umul_ppmm(a0, sb, b0, c0)` — 64×64 → 128-bit full product.
 *    In TS: `prod = b0 * c0` (BigInt handles arbitrary precision).
 *    `a0 = prod >> 64` (high 64 bits), `sb = prod & MASK64` (low 64 bits).
 * 3. Normalise: if MSB of `a0` is clear, shift left by 1 (grab MSB of `sb`
 *    into LSB of `a0`), shift `sb` left by 1, decrement `ax`.
 * 4. `rb = sb & HIGHBIT` (MSB of low limb = round bit).
 *    `sb = sb & ~HIGHBIT` (remaining low bits = sticky).
 * 5. Sign of result: sign(b) * sign(c).
 * 6. Overflow check (before rounding).
 * 7. Underflow check (per C logic with goto rounding bypass).
 * 8. Round per rounding mode with MPFR_LIMB_ONE = 1 (sh == 0).
 *
 * Ref: mpfr/src/mul.c L371-L461 — the C reference body.
 * Ref: mpfr/src/mul.c L812-L813 — dispatcher routes prec==64 here.
 * Ref: src/ops/add1sp1n.ts — parallel structure for addition (same prec=64 shape).
 * Ref: src/ops/overflow.ts — exp-overflow delegate.
 * Ref: src/ops/underflow.ts — exp-underflow delegate.
 * Ref: CLAUDE.md 'Hallucination-risk callouts: umul_ppmm output args are first'.
 * Ref: CLAUDE.md 'Hallucination-risk callouts: Ternary flag is sign of (rounded - exact)'.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from "../core.ts";
import { MPFRError } from "../core.ts";
import { mpfr_overflow } from "./overflow.ts";
import { mpfr_underflow } from "./underflow.ts";

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** GMP_NUMB_BITS — 64 bits per limb on x86_64. */
const GMP_NUMB_BITS = 64n;

/** All 64 bits set: used for 64-bit truncation of BigInt arithmetic. */
const LIMB_MASK_64 = (1n << GMP_NUMB_BITS) - 1n;

/**
 * MPFR_LIMB_HIGHBIT = 1 << 63 — the MSB of a 64-bit limb.
 * Ref: mpfr/src/mpfr-impl.h L1301 — MPFR_LIMB_HIGHBIT.
 */
const LIMB_HIGHBIT = 1n << 63n;

/**
 * MPFR_LIMB_ONE = 1 (since sh == 0 for prec == 64).
 * Ref: mpfr/src/mpfr-impl.h L1302 — MPFR_LIMB_ONE = (mp_limb_t)1 << sh.
 * For sh == 0 this collapses to 1.
 */
const LIMB_ONE = 1n;

/**
 * Default exponent ceiling — matches `__gmpfr_emax` on fresh init.
 * Ref: mpfr/src/mpfr.h L231 — MPFR_EMAX_DEFAULT = (1 << 30) - 1.
 */
const EMAX_DEFAULT = (1n << 30n) - 1n;

/**
 * Default exponent floor — matches `__gmpfr_emin` on fresh init.
 * Ref: mpfr/src/mpfr.h L232 — MPFR_EMIN_DEFAULT = -(MPFR_EMAX_DEFAULT).
 */
const EMIN_DEFAULT = -EMAX_DEFAULT;

/** Output precision for this fast path: exactly 64 bits. */
const OUTPUT_PREC = 64n;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ (mpfr-impl.h L1233-L1234):
 * true when `rnd` rounds toward zero with respect to the sign.
 *
 *   MPFR_IS_LIKE_RNDZ(rnd, neg) = rnd==RNDZ || (rnd+neg==RNDD)
 * where neg = (sign < 0).
 *
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

/**
 * TS analogue of MPFR_IS_LIKE_RNDA (rounds away from zero w.r.t sign).
 * Ref: mpfr/src/mpfr-impl.h — MPFR_IS_LIKE_RNDA is the complement of RNDZ.
 */
function isLikeRNDA(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDA') return true;
  if (rnd === 'RNDU' && sign === 1) return true;
  if (rnd === 'RNDD' && sign === -1) return true;
  return false;
}

/** Construct the output MPFR normal value from 64-bit limb arithmetic results. */
function buildResult(sign: Sign, exp: bigint, a0: bigint): MPFR {
  return {
    kind: 'normal',
    sign,
    prec: OUTPUT_PREC,
    exp,
    mant: a0,
  } satisfies MPFR;
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

const VALID_RND: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];

/**
 * Single-limb multiplication fast path for output precision == 64 bits.
 *
 * Mirrors `mpfr_mul_1n` from `mpfr/src/mul.c L371-L461`.
 *
 * @mpfrName mpfr_mul_1n
 *
 * @param b   First multiplicand; must be `kind='normal'`, `prec <= 64n`.
 * @param c   Second multiplicand; must be `kind='normal'`, `prec <= 64n`.
 * @param rnd Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }` — the correctly-rounded product at prec=64
 *          and the ternary flag (sign of rounded − exact).
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_mul_1n(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Input validation.
  // Ref: mpfr/src/mul.c L375-L377 — MPFR_MANT(b)[0], MPFR_MANT(c)[0].
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_mul_1n: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_mul_1n: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec > GMP_NUMB_BITS) {
    throw new MPFRError('EPREC', `mpfr_mul_1n: b.prec must be <= 64n, got ${b.prec}`);
  }
  if (c.prec > GMP_NUMB_BITS) {
    throw new MPFRError('EPREC', `mpfr_mul_1n: c.prec must be <= 64n, got ${c.prec}`);
  }
  if (!VALID_RND.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_mul_1n: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Extract inputs. In C:
  //   b0 = MPFR_MANT(b)[0]; c0 = MPFR_MANT(c)[0];
  //
  // In the TS schema, `mant` is MSB-aligned to `prec` bits. For prec <= 64,
  // the C limb b0 is the TS mant already (the MSB of the full 64-bit limb
  // is at bit position prec-1 of mant, but the C side stores this as a
  // 64-bit limb with the MSB at bit 63). We need the 64-bit C-limb form:
  // left-shift mant by (64 - prec) to align the MSB to bit 63.
  //
  // Ref: mpfr/src/mul.c L375-L376 — b0, c0 are 64-bit MPFR mantissa limbs.
  // Ref: mpfr/src/mpfr-impl.h L268-L272 — MSB-normalisation: top limb has MSB set.
  // -------------------------------------------------------------------------

  const b_shift = GMP_NUMB_BITS - b.prec;
  const c_shift = GMP_NUMB_BITS - c.prec;
  const b0: bigint = b.mant << b_shift;  // 64-bit C-limb form of b's mantissa
  const c0: bigint = c.mant << c_shift;  // 64-bit C-limb form of c's mantissa

  // -------------------------------------------------------------------------
  // Step 1: provisional exponent.
  // Ref: mpfr/src/mul.c L380 — ax = MPFR_GET_EXP(b) + MPFR_GET_EXP(c).
  // -------------------------------------------------------------------------

  let ax: bigint = b.exp + c.exp;

  // -------------------------------------------------------------------------
  // Step 2: 64×64 → 128-bit full product via BigInt.
  // Equivalent to `umul_ppmm(a0, sb, b0, c0)` — output args are FIRST.
  // Ref: mpfr/src/mul.c L381 — umul_ppmm(a0, sb, b0, c0).
  // Ref: CLAUDE.md hallucination callout — umul_ppmm: first two args are outputs.
  //
  // prod = b0 * c0 (128-bit full product)
  // a0 = prod >> 64 (high limb)
  // sb = prod & MASK64 (low limb)
  // -------------------------------------------------------------------------

  const prod: bigint = b0 * c0;
  let a0: bigint = prod >> GMP_NUMB_BITS;
  let sb: bigint = prod & LIMB_MASK_64;

  // -------------------------------------------------------------------------
  // Step 3: normalise.
  // Ref: mpfr/src/mul.c L382-L390.
  //
  // If the high limb doesn't have its MSB set, shift left by 1 (grab the
  // MSB of the low limb into the LSB of the high limb) and shift the low
  // limb left by 1, then decrement the exponent.
  //
  // This can only happen when b0 and c0 are not both at maximum magnitude
  // (i.e., not both 0xFFFF...FFFF). After normalisation, a0 always has
  // its MSB (bit 63) set.
  // -------------------------------------------------------------------------

  if (a0 < LIMB_HIGHBIT) {
    ax = ax - 1n;
    // a0 = (a0 << 1) | (sb >> 63)  — bring MSB of sb into LSB of a0
    a0 = ((a0 << 1n) | (sb >> (GMP_NUMB_BITS - 1n))) & LIMB_MASK_64;
    // sb <<= 1  — shift sticky accumulator left
    sb = (sb << 1n) & LIMB_MASK_64;
  }

  // -------------------------------------------------------------------------
  // Step 4: extract round bit and sticky.
  // Ref: mpfr/src/mul.c L391-L392.
  //
  //   rb = sb & MPFR_LIMB_HIGHBIT   — MSB of the low limb
  //   sb = sb & ~MPFR_LIMB_HIGHBIT  — remaining bits of low limb
  // -------------------------------------------------------------------------

  const rb: bigint = sb & LIMB_HIGHBIT;
  sb = sb & ~LIMB_HIGHBIT & LIMB_MASK_64;

  // -------------------------------------------------------------------------
  // Step 5: sign of result.
  // Ref: mpfr/src/mul.c L395 — MPFR_MULT_SIGN(sign_b, sign_c).
  //
  // MPFR_MULT_SIGN: product of two MPFR signs (1 or -1) gives the result sign.
  // -------------------------------------------------------------------------

  const sign: Sign = (b.sign * c.sign) as Sign;

  // -------------------------------------------------------------------------
  // Step 6: overflow check (pre-rounding).
  // Ref: mpfr/src/mul.c L398-L399.
  // -------------------------------------------------------------------------

  if (ax > EMAX_DEFAULT) {
    return mpfr_overflow(OUTPUT_PREC, rnd, sign);
  }

  // -------------------------------------------------------------------------
  // Step 7: underflow check.
  // Ref: mpfr/src/mul.c L408-L423.
  //
  // Check happens BEFORE rounding. There is a bypass ("goto rounding") for
  // cases where the result can still be normalised after rounding up.
  //
  // The underflow guard: if ax >= emin, no underflow.
  // If ax < emin:
  //   - If ax == emin - 1 and a0 == 0xFFFF...FFFF (all-ones, ~MPFR_LIMB_ZERO):
  //     check if rounding up would push back into range:
  //     (RNDN && rb) || (RNDA-like && (rb | sb)) → bypass to rounding.
  //   - Otherwise, possibly adjust rnd_mode to RNDZ for RNDN at very small
  //     values, then delegate to mpfr_underflow.
  // -------------------------------------------------------------------------

  let rnd_eff = rnd;  // effective rounding mode (may be adjusted for underflow)

  if (ax < EMIN_DEFAULT) {
    const all_ones = LIMB_MASK_64;  // ~MPFR_LIMB_ZERO in 64-bit

    // Check for the "no underflow" bypass.
    // Ref: mpfr/src/mul.c L410-L413.
    const bypass =
      ax === EMIN_DEFAULT - 1n &&
      a0 === all_ones &&
      ((rnd === 'RNDN' && rb !== 0n) ||
       (isLikeRNDA(rnd, sign) && (rb | sb) !== 0n));

    if (!bypass) {
      // Apply RNDN → RNDZ adjustment for very small values.
      // Ref: mpfr/src/mul.c L418-L421.
      if (rnd === 'RNDN' &&
          (ax < EMIN_DEFAULT - 1n ||
           (a0 === LIMB_HIGHBIT && (rb | sb) === 0n))) {
        rnd_eff = 'RNDZ';
      }
      return mpfr_underflow(OUTPUT_PREC, rnd_eff, sign);
    }
    // bypass: fall through to rounding with ax < emin (the MPFR_EXP assignment
    // below will use the adjusted ax; the result will then be valid because
    // rounding up pushes a0 to carry and ax back to emin).
  }

  // -------------------------------------------------------------------------
  // Step 8: rounding.
  // Ref: mpfr/src/mul.c L425-L460.
  //
  // MPFR_LIMB_ONE = 1 (sh == 0 since output prec == 64).
  // ternary convention: sign of (rounded - exact).
  //   truncate → ternary = -sign(a)
  //   add_one_ulp → ternary = +sign(a)
  // -------------------------------------------------------------------------

  if (rb === 0n && sb === 0n) {
    // Exact.
    // Ref: mpfr/src/mul.c L428 — MPFR_RET(0).
    return { value: buildResult(sign, ax, a0), ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd_eff === 'RNDN') {
    // Ref: mpfr/src/mul.c L433-L438.
    // Truncate if rb == 0 (below midpoint), or tie (rb != 0, sb == 0)
    // with even LSB (ap[0] & MPFR_LIMB_ONE == 0).
    const lsb = a0 & LIMB_ONE;  // MPFR_LIMB_ONE = 1 since sh == 0
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd_eff, sign)) {
    // Truncate.
    // Ref: mpfr/src/mul.c L440-L445.
    doAddOneUlp = false;
  } else {
    // Round away from zero.
    // Ref: mpfr/src/mul.c L446-L460.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // truncate: ternary = -MPFR_SIGN(a).
    // Ref: mpfr/src/mul.c L444 — MPFR_RET(-MPFR_SIGN(a)).
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value: buildResult(sign, ax, a0), ternary };
  }

  // add_one_ulp branch.
  // Ref: mpfr/src/mul.c L448-L459.
  //
  // ap[0] += MPFR_LIMB_ONE (= 1 since sh == 0).
  // If ap[0] overflows to 0 (all-bits-set → carry):
  //   ap[0] = HIGHBIT, ax++, check overflow.

  let newA0 = (a0 + LIMB_ONE) & LIMB_MASK_64;
  let newAx = ax;

  if (newA0 === 0n) {
    // Mantissa carry-out: ulp increment rippled all the way through.
    // Ref: mpfr/src/mul.c L450-L458.
    newA0 = LIMB_HIGHBIT;
    newAx = ax + 1n;
    if (newAx > EMAX_DEFAULT) {
      return mpfr_overflow(OUTPUT_PREC, rnd, sign);
    }
  }

  // Ref: mpfr/src/mul.c L459 — MPFR_RET(MPFR_SIGN(a)).
  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  return { value: buildResult(sign, newAx, newA0), ternary };
}
