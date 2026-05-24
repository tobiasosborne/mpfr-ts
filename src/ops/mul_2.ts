/**
 * ops/mul_2.ts — pure-TS port of MPFR's `mpfr_mul_2`.
 *
 * Two-limb multiplication fast path for the target precision range
 * `GMP_NUMB_BITS < p < 2 * GMP_NUMB_BITS`, i.e. 65 <= p <= 127 on x86_64.
 * Both operands must have the same precision as the target.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_mul_2(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                          mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_mul_2(b, c, prec, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS)
 * ----------------------------------------------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.prec === c.prec === prec` (same-prec fast path)
 *   - `64n < prec < 128n` (i.e. 65..127 bits, the two-limb range)
 *
 * Algorithm
 * ---------
 *
 * In C, the function uses three `umul_ppmm` calls to assemble a 4-limb
 * product from two 2-limb inputs (each 128-bit MSB-aligned). The top
 * two limbs go into `ap[1]:ap[0]`; the lower bits provide the sticky/round
 * tail.
 *
 * In the TS port we exploit native BigInt to compute the full product
 * directly. The product `b.mant * c.mant` has exactly `2*p` bits of
 * significance (since each mant is in [2^(p-1), 2^p)). We then:
 *
 *   1. Compute the exact product as a bigint.
 *   2. Determine the result exponent: ax = b.exp + c.exp, adjusted
 *      by -1 if the product is not in [2^(2p-1), 2^(2p)) — i.e. the
 *      product fell one bit short (MSB at position 2p-2).
 *      Ref: mpfr/src/mul.c L512-L519 — the `if (h < MPFR_LIMB_HIGHBIT)`
 *      shift left by 1 case.
 *   3. Round the `2p`-bit product to `p` bits via `roundMantissa`.
 *   4. Check for overflow post-rounding.
 *
 * The result sign is `b.sign * c.sign`.
 *
 * Ref: mpfr/src/mul.c L469-L588 — the C reference body (64<prec<128 case).
 * Ref: mpfr/src/mul.c L810 — dispatcher routes here.
 * Ref: src/ops/add1sp2.ts — parallel two-limb same-prec arithmetic.
 * Ref: src/ops/overflow.ts — exp-overflow delegate.
 * Ref: CLAUDE.md "Hallucination-risk callouts: umul_ppmm output args are first".
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from "../core.ts";
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posInf,
  negInf,
} from "../core.ts";
import { roundMantissa } from "../internal/mpfr/round_raw.ts";
import { mpfr_setmax } from "./setmax.ts";

// ---------------------------------------------------------------------------
// Constants
// Ref: mpfr/src/mpfr-impl.h L1300-L1311
// ---------------------------------------------------------------------------

/** 64-bit limb width: GMP_NUMB_BITS on x86_64. */
const GMP_NUMB_BITS = 64n;

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
 * Produce an overflow result.
 * Ref: mpfr/src/exceptions.c L424-L448 — mpfr_overflow body.
 * Delegates to mpfr_setmax for the toward-zero branch, or returns ±Inf.
 */
function handleOverflow(prec: bigint, rnd: RoundingMode, sign: Sign): Result {
  if (isLikeRNDZ(rnd, sign)) {
    // Rounds toward zero → ±max-finite.
    // Ref: mpfr/src/exceptions.c L437 — mpfr_setmax(x, __gmpfr_emax).
    const value = mpfr_setmax(prec, EMAX_DEFAULT, sign);
    // Ternary: -1 for positive (rounded < exact overflow), +1 for negative.
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value, ternary };
  }
  // Rounds away from zero → ±Inf.
  // Ref: mpfr/src/exceptions.c L441 — MPFR_SET_INF(x).
  const value = sign === 1 ? posInf(prec) : negInf(prec);
  // Ternary: +1 for positive (∞ > exact), -1 for negative (−∞ < exact).
  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  return { value, ternary };
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

const VALID_RND: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];

function validateArgs(b: MPFR, c: MPFR, prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (!VALID_RND.includes(rnd)) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_mul_2: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_mul_2: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== prec) {
    throw new MPFRError('EPREC', `mpfr_mul_2: b.prec (${b.prec}) !== prec (${prec})`);
  }
  if (c.prec !== prec) {
    throw new MPFRError('EPREC', `mpfr_mul_2: c.prec (${c.prec}) !== prec (${prec})`);
  }
  if (prec <= GMP_NUMB_BITS || prec >= 2n * GMP_NUMB_BITS) {
    throw new MPFRError(
      'EPREC',
      `mpfr_mul_2: prec must be in (64, 128), got ${prec}`,
    );
  }
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Two-limb multiplication fast path for 65 <= prec <= 127.
 *
 * @mpfrName mpfr_mul_2
 *
 * @param b    First factor; must be `kind='normal'`, same `prec` as target.
 * @param c    Second factor; must be `kind='normal'`, same `prec` as target.
 * @param prec Target precision in bits; must be in (64, 128).
 * @param rnd  Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }`.
 *
 * @throws {MPFRError} `EPREC` on bad inputs; `EROUND` on bad rnd.
 */
export function mpfr_mul_2(
  b: MPFR,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions.
  // Ref: mpfr/src/mul.c L469-L477 — MPFR_ASSERTD checks.
  // -------------------------------------------------------------------------
  validateArgs(b, c, prec, rnd);

  // -------------------------------------------------------------------------
  // Compute result sign.
  // Ref: mpfr/src/mul.c L525 — MPFR_SIGN(a) = MPFR_MULT_SIGN(MPFR_SIGN(b), MPFR_SIGN(c)).
  // -------------------------------------------------------------------------
  const resultSign: Sign = (b.sign * c.sign) as Sign;

  // -------------------------------------------------------------------------
  // Compute initial exponent.
  // Ref: mpfr/src/mul.c L474 — ax = MPFR_GET_EXP(b) + MPFR_GET_EXP(c).
  // -------------------------------------------------------------------------
  let ax = b.exp + c.exp;

  // -------------------------------------------------------------------------
  // Compute the exact product.
  //
  // In C: the two 2-limb mantissas (bp[1]:bp[0] and cp[1]:cp[0]) are both
  // MSB-aligned to 128 bits. Their product h:l:sb:sb2 occupies 256 bits.
  //
  // In TS: b.mant is in [2^(p-1), 2^p) and c.mant is in [2^(p-1), 2^p).
  // The exact product is in [2^(2p-2), 2^(2p)).
  //
  // The C code assembles a partial product first (only the top 3 limbs of
  // the 4-limb result) and checks if the low bits of l allow early rounding
  // without computing b0*c0. In TS with BigInt, we always compute the full
  // product — this is equivalent to always computing the exact b0*c0 path
  // in the C fallback.
  //
  // Ref: mpfr/src/mul.c L480-L511 — product assembly and approximate check.
  // -------------------------------------------------------------------------
  const product = b.mant * c.mant;

  // -------------------------------------------------------------------------
  // Determine the actual bit-length of the product.
  //
  // The product is in [2^(2p-2), 2^(2p)). Its bit-length L is either 2p
  // (if product >= 2^(2p-1)) or 2p-1 (if product < 2^(2p-1)).
  //
  // This mirrors the C code's `if (h < MPFR_LIMB_HIGHBIT)` check:
  //   h = the top 64-bit limb of the 128-bit top half of the product.
  //   h < MPFR_LIMB_HIGHBIT means the product's MSB is at position 2p-2,
  //   so we shift left 1 and decrement ax.
  // Ref: mpfr/src/mul.c L512-L519 — left-shift-by-1 normalization.
  // -------------------------------------------------------------------------
  const fullBits = 2n * prec;
  const highBit = 1n << (fullBits - 1n);  // 2^(2p-1)
  let srcPrec: bigint;

  if (product >= highBit) {
    // MSB is at position 2p-1 (0-indexed): bit-length is 2p. No adjustment.
    srcPrec = fullBits;
  } else {
    // MSB is at position 2p-2: bit-length is 2p-1. Decrement exponent.
    // In C: ax--, h <<= 1 (shift left), etc.
    ax -= 1n;
    srcPrec = fullBits - 1n;
    // Note: the product mantissa is now effectively shifted left by 1
    // from C's perspective, but roundMantissa works with the raw bigint
    // which is MSB-aligned to srcPrec bits — so we just set srcPrec
    // to 2p-1 and roundMantissa will extract correctly.
  }

  // -------------------------------------------------------------------------
  // Overflow check (pre-rounding).
  // Ref: mpfr/src/mul.c L528-L529 — if (ax > __gmpfr_emax) return mpfr_overflow.
  // Note: We do not check underflow (per spec.json divergence note).
  // -------------------------------------------------------------------------
  if (ax > EMAX_DEFAULT) {
    return handleOverflow(prec, rnd, resultSign);
  }

  // -------------------------------------------------------------------------
  // Round to target precision.
  //
  // `product` is an unsigned bigint in [2^(srcPrec-1), 2^srcPrec).
  // `roundMantissa` drops the low (srcPrec - prec) bits and computes the
  // ternary flag.
  //
  // Ref: mpfr/src/mul.c L520-L587 — rb/sb extraction and rounding logic.
  // -------------------------------------------------------------------------
  if (srcPrec === prec) {
    // Exact (no bits to drop) — can only happen if product is an exact p-bit
    // integer, which would require very specific inputs. Handle losslessly.
    const value: MPFR = {
      kind: 'normal',
      sign: resultSign,
      prec,
      exp: ax,
      mant: product,
    };
    return { value, ternary: 0 };
  }

  // srcPrec > prec always here (srcPrec is 2p-1 or 2p, prec is in (64,128)).
  const { mant, exp: roundedExp, ternary: rawTernary } = roundMantissa(
    product,
    srcPrec,
    ax,
    prec,
    resultSign,
    rnd,
  );

  // -------------------------------------------------------------------------
  // Post-rounding overflow check.
  // Rounding can bump the exponent by 1 (when the mantissa carries out).
  // Ref: mpfr/src/mul.c L574-L587 — add_one_ulp carries.
  // -------------------------------------------------------------------------
  if (roundedExp > EMAX_DEFAULT) {
    return handleOverflow(prec, rnd, resultSign);
  }

  const ternary: Ternary = rawTernary;
  const value: MPFR = {
    kind: 'normal',
    sign: resultSign,
    prec,
    exp: roundedExp,
    mant,
  };
  return { value, ternary };
}
