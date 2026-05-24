/**
 * ops/div_2.ts -- pure-TS port of MPFR's `mpfr_div_2`.
 *
 * Two-limb division fast path: prec(q) == prec(u) == prec(v) = p, where
 * GMP_NUMB_BITS (64) < p < 2 * GMP_NUMB_BITS (128), i.e. 65 <= p <= 127.
 * The dispatcher in mpfr_div routes here when those conditions hold.
 *
 * C signature:
 *   static int mpfr_div_2(mpfr_ptr q, mpfr_srcptr u, mpfr_srcptr v,
 *                         mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port):
 *   mpfr_div_2(u, v, prec, rnd) -> Result
 *
 * Ref: mpfr/src/div.c L390-L643 -- the C reference body.
 * Ref: mpfr/src/div.c L841-L843 -- dispatcher routes here when
 *      GMP_NUMB_BITS < prec < 2*GMP_NUMB_BITS.
 *
 * Algorithm
 * ---------
 *
 * The C source uses a Moller-Granlund 2-limb approximate-inverse
 * (mpfr_div2_approx, mpfr/src/div.c L256-L389) plus a sub_ddmmss-driven
 * fixup loop to compute the quotient. The TS port sidesteps all of that:
 * BigInt exact division gives the same result in one operation.
 *
 * Entry conditions (dispatcher enforces, we may assert):
 *   - u.kind === 'normal', v.kind === 'normal'
 *   - u.prec === v.prec === prec
 *   - 65n <= prec <= 127n
 *
 * Steps:
 *
 * 1. Sign: resultSign = u.sign * v.sign.
 *    Ref: mpfr/src/div.c L581 -- MPFR_MULT_SIGN(MPFR_SIGN(u), MPFR_SIGN(v)).
 *
 * 2. Preliminary exponent: qx0 = u.exp - v.exp.
 *    Ref: mpfr/src/div.c L397 -- qx = MPFR_GET_EXP(u) - MPFR_GET_EXP(v).
 *
 * 3. Extra flag: extra = (u.mant >= v.mant).
 *    This mirrors the C check at L406:
 *      extra = r3 > v1 || (r3 == v1 && r2 >= v0)
 *    where r3:r2 = u.mant[1]:u.mant[0] (MSB-first pseudoregisters for the
 *    2-limb mantissa). Since u.mant and v.mant are the same bigint recomputed
 *    from those limbs, `extra = (u.mant >= v.mant)` is equivalent.
 *    Ref: mpfr/src/div.c L406-L408.
 *
 *    IMPORTANT: The mantissas here are MSB-aligned prec-bit bigints. For a
 *    2-limb value, the in-memory layout is LITTLE-ENDIAN by limb index
 *    (CLAUDE.md "GMP mpn limbs are LITTLE-ENDIAN by limb index"), so
 *    mant[0] = LSB word, mant[1] = MSB word. The bigint `mant` already
 *    captures both limbs correctly; no reconstruction needed.
 *
 * 4. Subtract if extra: u_mant_adj = extra ? u.mant - v.mant : u.mant.
 *    After this step, u_mant_adj < v.mant.
 *    Ref: mpfr/src/div.c L408 -- sub_ddmmss(r3, r2, r3, r2, v1, v0).
 *
 * 5. Compute the 128-bit quotient via BigInt:
 *      q_raw = floor(u_mant_adj * 2^(2*GMP) / v.mant)
 *      rem_raw = u_mant_adj * 2^(2*GMP) mod v.mant
 *    q_raw is at most 128 bits (since u_mant_adj < v.mant means ratio < 1,
 *    times 2^128 gives < 2^128).
 *    This replaces the Moller-Granlund Newton-refinement + fixup loop.
 *    Ref: spec.json doc -- "bigint division ((u_combined << (2*64+1)) / v_combined)".
 *    (The spec mentions 2*64+1 as a possible shift; we use 2*64 here since we
 *    handle the +1 via the extra flag normalization step that follows. The
 *    results are equivalent: the shift of 2*64+1 with a later right-shift
 *    equals our shift of 2*64 before the normalization step.)
 *
 * 6. For the extra=1 case (C's L569-L575), normalize the quotient:
 *      The C code right-shifts q1:q0 by 1 and sets q1's HIGHBIT.
 *      In BigInt terms: q = (q_raw >> 1) | (1n << (2*GMP - 1))
 *      Also: sb |= q_raw & 1 (the shifted-out bit contributes to sticky).
 *      Ref: mpfr/src/div.c L569-L575.
 *
 * 7. Extract round-bit (rb) and sticky-bit (sb):
 *    sh = 2*GMP - prec  (number of tail bits in a 2-limb 128-bit mantissa)
 *    mask = (1 << sh) - 1
 *    rb = q & (1 << (sh-1))
 *    sb |= (q & mask) ^ rb_bit  (the bits below rb in the tail)
 *    Also sb |= (rem_raw != 0)
 *    Ref: mpfr/src/div.c L576-L577.
 *
 * 8. Truncate the result mantissa:
 *    resultMant = q >> sh  (top prec bits, MSB at position prec-1)
 *    The MSB is guaranteed set: C does qp[1] = q1; qp[0] = q0 & ~mask.
 *    Ref: mpfr/src/div.c L578-L579.
 *
 * 9. Apply rounding (5 MPFR modes) and compute ternary.
 *    Ref: mpfr/src/div.c L583-L642.
 *
 * 10. Overflow/underflow delegation.
 *     Ref: mpfr/src/div.c L584-L608.
 *
 * Ternary direction: sign of (rounded - exact). For positive sign,
 * truncation -> ternary=-1; increment -> ternary=+1.
 * Ref: CLAUDE.md "Ternary flag is the sign of (rounded - exact)".
 *
 * No signed zero can result here (both inputs are 'normal'), no NaN.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from '../core.ts';
import { mpfr_overflow } from './overflow.ts';
import { mpfr_underflow } from './underflow.ts';

/** GMP_NUMB_BITS = 64. Ref: gmp.h / gmp-impl.h. */
const GMP_NUMB_BITS: bigint = 64n;

/** Default emax. Ref: mpfr/src/mpfr.h L231. */
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n;
/** Default emin. Ref: mpfr/src/mpfr.h L232. */
const EMIN_DEFAULT: bigint = -((1n << 30n) - 1n);

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ.
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  return false;
}

function validateArgs(prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Two-limb division fast path for prec(q) = prec(u) = prec(v) = p,
 * where GMP_NUMB_BITS < p < 2*GMP_NUMB_BITS (65 <= p <= 127).
 *
 * Operates on MPFR normal values only. The caller (mpfr_div dispatcher)
 * is responsible for routing special cases (NaN, Inf, zero) elsewhere;
 * this function defensively checks and throws on singular inputs per
 * Rule 1 ("Fail fast, fail loud").
 *
 * @mpfrName mpfr_div_2
 *
 * @param u    Dividend -- kind must be 'normal', 65 <= prec <= 127.
 * @param v    Divisor  -- kind must be 'normal', same prec as u.
 * @param prec Target precision in bits (= u.prec = v.prec), in [65, 127].
 * @param rnd  Rounding mode.
 *
 * @returns {value, ternary} where value passes validate() and ternary is
 *          sign(rounded - exact).
 *
 * @throws {MPFRError} EPREC on bad prec; EROUND on bad rounding mode;
 *         EDOMAIN if either input is not 'normal' (caller should not do this).
 *
 * Ref: mpfr/src/div.c L390-L643 -- the C reference body.
 */
export function mpfr_div_2(
  u: MPFR,
  v: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // Defensive check on singular inputs (Rule 1 -- fail fast, fail loud).
  // The dispatcher (mpfr_div) filters all NaN/Inf/zero before calling here.
  if (u.kind !== 'normal' || v.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_div_2: expected both inputs to be 'normal', got '${u.kind}' and '${v.kind}'`,
    );
  }

  // Step 1: Sign.
  // Ref: mpfr/src/div.c L581.
  const resultSign: Sign = (u.sign * v.sign) as Sign;

  // Step 2: Preliminary exponent.
  // Ref: mpfr/src/div.c L397.
  const qx0: bigint = u.exp - v.exp;

  const uMant: bigint = u.mant;
  const vMant: bigint = v.mant;

  // Step 3: Extra flag (are the mantissas >= v?).
  // Ref: mpfr/src/div.c L406-L408.
  // The mantissa bigints are MSB-aligned prec-bit values; comparing them
  // directly is equivalent to comparing the 2-limb big-endian pairs
  // (r3, r2) and (v1, v0) used in the C code.
  const extra: boolean = uMant >= vMant;

  // Step 4: Adjust dividend.
  // After this, uMantAdj < vMant.
  // Ref: mpfr/src/div.c L408.
  const uMantAdj: bigint = extra ? uMant - vMant : uMant;

  // Step 5: Compute 128-bit quotient via exact BigInt division.
  // We compute: floor(uMantAdj * 2^(2*GMP_NUMB_BITS) / vMant)
  // Since uMantAdj < vMant, the quotient is < 2^(2*GMP_NUMB_BITS) = 2^128.
  // Ref: spec.json doc -- "bigint division gives the exact quotient".
  // Ref: mpfr/src/div.c L413 (64-bit path) or L480-L555 (general path).
  const TWO_LIMBS: bigint = 2n * GMP_NUMB_BITS;  // = 128
  const qRaw: bigint = (uMantAdj << TWO_LIMBS) / vMant;
  const remRaw: bigint = (uMantAdj << TWO_LIMBS) - qRaw * vMant;

  // sh = 2*GMP_NUMB_BITS - prec = number of "tail bits" in the 2-limb mantissa.
  // Ref: mpfr/src/div.c L398.
  const sh: bigint = TWO_LIMBS - prec;

  // Step 6: Normalize for extra=1.
  // C code (L569-L575):
  //   qx++
  //   sb |= q0 & 1                              [dropped LSB contributes to sticky]
  //   q0 = (q1 << (GMP_NUMB_BITS - 1)) | (q0 >> 1)  [right shift 128-bit q by 1]
  //   q1 = MPFR_LIMB_HIGHBIT | (q1 >> 1)        [set bit 127]
  // In BigInt: q = (qRaw >> 1) | (1n << (TWO_LIMBS - 1)), sb |= qRaw & 1n.
  let q: bigint;
  let sbFromExtra: bigint;
  let qx: bigint;

  if (extra) {
    qx = qx0 + 1n;
    sbFromExtra = (qRaw & 1n) | (remRaw !== 0n ? 1n : 0n);
    q = (qRaw >> 1n) | (1n << (TWO_LIMBS - 1n));
  } else {
    qx = qx0;
    sbFromExtra = remRaw !== 0n ? 1n : 0n;
    q = qRaw;
  }

  // Step 7: Extract round-bit and sticky-bit.
  // Ref: mpfr/src/div.c L576-L577:
  //   rb = q0 & (MPFR_LIMB_ONE << (sh - 1))
  //   sb |= (q0 & mask) ^ rb
  // where mask = MPFR_LIMB_MASK(sh) = (1 << sh) - 1.
  const rbBit: bigint = 1n << (sh - 1n);
  const mask: bigint = (1n << sh) - 1n;

  // rb is the actual bit value (0 or rbBit), matching the C `rb = q0 & (MPFR_LIMB_ONE << (sh-1))`.
  // sb |= (q0 & mask) ^ rb means: start with all tail bits, then XOR out the rb bit itself.
  // This leaves only the bits strictly below the round bit.
  // Ref: mpfr/src/div.c L576-L577.
  const rbActual: bigint = q & rbBit;          // 0n or rbBit
  const rb: boolean = rbActual !== 0n;
  const sbLow: bigint = (q & mask) ^ rbActual; // (q & mask) with the rb bit cleared
  const sb: boolean = (sbLow | sbFromExtra) !== 0n;

  // Step 8: Truncate to prec bits.
  // C code (L578-L579):
  //   qp[1] = q1
  //   qp[0] = q0 & ~mask
  // In BigInt: resultMant = q >> sh  (drops the tail sh bits, leaving prec bits).
  // The MSB (bit prec-1) is guaranteed to be set since we normalized above.
  // Ref: mpfr/src/div.c L578-L579.
  const resultMant: bigint = q >> sh;

  // Step 9a: Overflow check (before underflow and rounding).
  // Ref: mpfr/src/div.c L584-L585.
  if (qx > EMAX_DEFAULT) {
    return mpfr_overflow(prec, rnd, resultSign);
  }

  // Step 9b: Underflow check.
  // C comment (L586-L609) and code:
  //   if qx < emin -> check RNDN special case, then call mpfr_underflow.
  // For RNDN: if qx < emin-1 OR (mantissa == HIGHBIT && sb == 0), use RNDZ.
  //   Note: the C code checks qp[1] == MPFR_LIMB_HIGHBIT && qp[0] == 0 && sb == 0.
  //   In BigInt: resultMant == (1n << (prec - 1n)) && sb == false.
  // Ref: mpfr/src/div.c L590-L608.
  if (qx < EMIN_DEFAULT) {
    let rndForUnderflow = rnd;
    if (rnd === 'RNDN') {
      const mantIsHighbitOnly: boolean = resultMant === (1n << (prec - 1n));
      if (qx < EMIN_DEFAULT - 1n || (mantIsHighbitOnly && !sb)) {
        rndForUnderflow = 'RNDZ';
      }
    }
    return mpfr_underflow(prec, rndForUnderflow, resultSign);
  }

  // Step 9c: Rounding.
  // Ref: mpfr/src/div.c L613-L642.
  if (!rb && !sb) {
    // Exact result.
    // Ref: mpfr/src/div.c L613-L617.
    return {
      value: {
        kind: 'normal',
        sign: resultSign,
        prec,
        exp: qx,
        mant: resultMant,
      },
      ternary: 0,
    };
  }

  // Determine whether to increment (round up in magnitude) or truncate.
  let increment: boolean;
  if (rnd === 'RNDN') {
    // RNDN: if rb=0, truncate; if rb=1, round up.
    // (The midpoint tie-to-even case is impossible for division of p-bit / p-bit
    // numbers -- sb is guaranteed nonzero. Ref: mpfr/src/div.c L620-L621 comment.)
    // Ref: mpfr/src/div.c L618-L625.
    increment = rb;
  } else if (isLikeRNDZ(rnd, resultSign)) {
    // RNDZ, or RNDD with positive sign, or RNDU with negative sign: truncate.
    // Ref: mpfr/src/div.c L627-L631.
    increment = false;
  } else {
    // Round away from zero: RNDA, RNDU with positive sign, RNDD with negative sign.
    // Ref: mpfr/src/div.c L633-L642.
    increment = true;
  }

  if (!increment) {
    // Truncate: rounded value is smaller in magnitude than exact.
    // Ternary = sign(rounded - exact). For positive: rounded < exact -> ternary = -1.
    // Ref: mpfr/src/div.c L631 -- MPFR_RET(-MPFR_SIGN(q)).
    const ternary = (resultSign === 1 ? -1 : 1) as -1 | 0 | 1;
    return {
      value: {
        kind: 'normal',
        sign: resultSign,
        prec,
        exp: qx,
        mant: resultMant,
      },
      ternary,
    };
  }

  // Add one ulp to resultMant (increment the LSB of the prec-bit mantissa).
  // C code (L636): qp[0] += MPFR_LIMB_ONE << sh; qp[1] += (qp[0] == 0).
  // The C assertion at L640 confirms no carry can produce a zero qp[1],
  // i.e. the incremented mantissa never overflows 2^prec. In prec-bit terms
  // the ulp is 1 (LSB of the mantissa); the same argument applies.
  // Ref: mpfr/src/div.c L636-L641.
  const incremented: bigint = resultMant + 1n;
  const upperBound: bigint = 1n << prec;

  if (incremented >= upperBound) {
    // Defensive carry handling (C asserts this cannot happen, but protect anyway).
    const ternary = (resultSign === 1 ? 1 : -1) as -1 | 0 | 1;
    return {
      value: {
        kind: 'normal',
        sign: resultSign,
        prec,
        exp: qx + 1n,
        mant: upperBound >> 1n,
      },
      ternary,
    };
  }

  // Rounded up: rounded value is larger in magnitude than exact.
  // Ternary = sign(rounded - exact). For positive: rounded > exact -> ternary = +1.
  // Ref: mpfr/src/div.c L641 -- MPFR_RET(MPFR_SIGN(q)).
  const ternary = (resultSign === 1 ? 1 : -1) as -1 | 0 | 1;
  return {
    value: {
      kind: 'normal',
      sign: resultSign,
      prec,
      exp: qx,
      mant: incremented,
    },
    ternary,
  };
}
