/**
 * ops/div_1n.ts — pure-TS port of MPFR's `mpfr_div_1n`.
 *
 * Single-limb division fast path for the case where the output precision
 * is exactly GMP_NUMB_BITS (= 64 bits), and both operand precisions are
 * also <= 64 bits. This is distinct from `mpfr_div_1` in that there is
 * no shift `sh` — the quotient fills the limb completely, so the round
 * and sticky bits are extracted differently.
 *
 * C signature:
 *
 *   static int mpfr_div_1n(mpfr_ptr q, mpfr_srcptr u, mpfr_srcptr v,
 *                          mpfr_rnd_t rnd_mode);
 *
 * TS signature (this port):
 *
 *   mpfr_div_1n(u: MPFR, v: MPFR, rnd: RoundingMode): Result
 *
 * Output precision is fixed at 64n (GMP_NUMB_BITS). The C side reads
 * this from the pre-allocated `q`; we produce it from scratch.
 *
 * Algorithm
 * ---------
 *
 * Ref: mpfr/src/div.c L256-L388.
 *
 * Let:
 *   u0 = u.mant  (64-bit MSB-aligned mantissa, i.e. in [2^63, 2^64))
 *   v0 = v.mant  (same range)
 *   qx = u.exp - v.exp  (preliminary result exponent)
 *
 * Step 1 — Normalise dividend:
 *   extra = (u0 >= v0) ? 1 : 0
 *   if extra: u0 -= v0
 *
 * Step 2 — Compute floor quotient q0 and remainder l such that:
 *   u0 * 2^64 = q0 * v0 + l,   0 <= l < v0
 *
 * In TypeScript we use BigInt division directly (no invert_limb_approx
 * approximation needed — we have arbitrary-precision integers):
 *   q0 = (u0 * LIMB_BASE) / v0    (BigInt floor division)
 *   l  = (u0 * LIMB_BASE) % v0
 *
 * Step 3 — Extract rb (round bit) and sb (sticky bit):
 *
 *   if extra == 0:
 *     qp[0] = q0                              (q0 fills the 64-bit limb)
 *     rb = (l + l overflows OR l + l >= v0)   (i.e., 2*l >= v0)
 *     sb = (rb ? l + l - v0 : l)              (remainder after halving test)
 *     [Note: since we only need sb's zero/nonzero status for rounding,
 *      we use sb = (2*l != v0) when rb=1, or sb = l when rb=0]
 *
 *   if extra == 1:
 *     qp[0] = MPFR_LIMB_HIGHBIT | (q0 >> 1)  (= 2^63 | (q0 >> 1))
 *     rb = q0 & 1                             (LSB of q0 is the round bit)
 *     sb = l                                  (remainder is the sticky)
 *     qx += 1                                 (exponent correction)
 *
 * Ref: mpfr/src/div.c L309-L323 — the rb/sb extraction branches.
 *
 * Step 4 — Sign, overflow check, rounding:
 *   Same structure as mpfr_div_1. Sign = product of input signs.
 *   Overflow: if qx > EMAX_DEFAULT → mpfr_overflow delegate.
 *   Underflow: omitted per spec.json divergence note.
 *   Rounding: the standard RNDN/RNDZ/RNDU/RNDD/RNDA decision tree.
 *
 * Key invariants from the C source:
 *   - For RNDN, rb != 0 implies sb != 0 (a 64-bit/64-bit division
 *     cannot produce an exact 65-bit quotient). So RNDN never hits
 *     the midpoint case (rb=1, sb=0); it always rounds up on rb=1.
 *     Ref: mpfr/src/div.c L361-L371.
 *   - The add_one_ulp branch for div_1n adds 1 to qp[0] (no mask
 *     needed since sh=0). No carry can occur — same proof as div_1.
 *     Ref: mpfr/src/div.c L381-L386.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/div.c L256-L388  — complete mpfr_div_1n body.
 *   - mpfr/src/div.c L846       — dispatcher routes prec=64 here.
 *   - mpfr/src/div.c L309-L323  — rb/sb extraction per extra flag.
 *   - mpfr/src/div.c L361-L371  — RNDN cannot be at midpoint.
 *   - mpfr/src/div.c L379-L386  — add_one_ulp adds 1 (no mask).
 *   - src/ops/div.ts             — general mpfr_div (structural reference).
 *   - src/ops/overflow.ts        — overflow delegate.
 *   - CLAUDE.md "Ternary flag is sign of (rounded - exact)".
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  NAN_VALUE as _NAN_VALUE,
  PREC_MIN,
  PREC_MAX,
  negInf,
  negZero,
  posInf,
  posZero,
} from '../core.ts';
import { mpfr_overflow } from './overflow.ts';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** Output precision: GMP_NUMB_BITS = 64.
 *  Ref: mpfr/src/div.c L265 — MPFR_ASSERTD(MPFR_PREC(q) == GMP_NUMB_BITS). */
const PREC_64: bigint = 64n;

/** 2^64 — the base for the 64-bit limb system.
 *  Used as LIMB_BASE when computing u0 * 2^64 / v0. */
const LIMB_BASE: bigint = 1n << 64n;

/** MPFR_LIMB_HIGHBIT = 2^63. Used in the extra=1 branch.
 *  Ref: mpfr/src/div.c L319 — qp[0] = MPFR_LIMB_HIGHBIT | (q0 >> 1). */
const HIGHBIT: bigint = 1n << 63n;

/** Default exponent ceiling. MPFR_EMAX_DEFAULT = (1 << 30) - 1.
 *  Ref: mpfr/src/mpfr.h L231. */
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Product-of-signs: mirrors MPFR_MULT_SIGN(a, b).
 * Ref: mpfr/src/div.c L325 — MPFR_SIGN(q) = MPFR_MULT_SIGN(MPFR_SIGN(u), MPFR_SIGN(v)).
 */
function multSign(a: Sign, b: Sign): Sign {
  return (a * b) as Sign;
}

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ — true when the rounding mode rounds
 * toward zero with respect to the given sign.
 *
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 *   RNDZ always; RNDU when sign<0 (rounds away from -inf, i.e. toward zero);
 *   RNDD when sign>0 (rounds toward -inf, which is toward zero for positives).
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Single-limb division fast path: prec(q) == 64, prec(u), prec(v) <= 64.
 *
 * @mpfrName mpfr_div_1n
 *
 * @param u   Dividend. Must be kind:'normal' with prec=64n (enforced by the
 *            caller/dispatcher; this function is a fast path, not a general
 *            handler — NaN/Inf/zero are handled by mpfr_div upstream).
 * @param v   Divisor. Same constraints as u.
 * @param rnd Rounding mode (one of the five standard modes).
 *
 * @returns   `{ value, ternary }` at prec=64n.
 *
 * @throws {MPFRError} `EPREC` if prec args are bad; `EROUND` if rnd is bad.
 *
 * @example
 *   // 1.0 / 1.0 = 1.0, exact
 *   mpfr_div_1n(u1, v1, 'RNDN');  // → { value: 1.0 @ prec64, ternary: 0 }
 */
export function mpfr_div_1n(
  u: MPFR,
  v: MPFR,
  rnd: RoundingMode,
): Result {
  // Validate rounding mode (prec is fixed at 64, no caller-supplied prec arg).
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `mpfr_div_1n: unknown rounding mode: ${String(rnd)}`);
  }

  // --- NaN/Inf/Zero specials (pass-through like mpfr_div) -----------------
  // These mirror the dispatch logic in mpfr_div for the normal caller path.
  // Ref: mpfr/src/div.c L783-L831 — special-case dispatch in mpfr_div.

  if (u.kind === 'nan' || v.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  if (u.kind === 'inf') {
    if (v.kind === 'inf') {
      return { value: NAN_VALUE, ternary: 0 };
    }
    // Inf / finite
    const s = multSign(u.sign, v.sign);
    return { value: s === 1 ? posInf(PREC_64) : negInf(PREC_64), ternary: 0 };
  }
  if (v.kind === 'inf') {
    // finite / Inf = ±0
    const s = multSign(u.sign, v.sign);
    return { value: s === 1 ? posZero(PREC_64) : negZero(PREC_64), ternary: 0 };
  }

  if (v.kind === 'zero') {
    if (u.kind === 'zero') {
      return { value: NAN_VALUE, ternary: 0 };
    }
    // finite-nonzero / 0 = ±Inf (divbyzero)
    const s = multSign(u.sign, v.sign);
    return { value: s === 1 ? posInf(PREC_64) : negInf(PREC_64), ternary: 0 };
  }
  if (u.kind === 'zero') {
    // 0 / finite-nonzero = ±0
    const s = multSign(u.sign, v.sign);
    return { value: s === 1 ? posZero(PREC_64) : negZero(PREC_64), ternary: 0 };
  }

  // --- Normal / Normal core -----------------------------------------------
  // Both u and v are kind:'normal'. Preconditions per C:
  //   MPFR_PREC(q) == GMP_NUMB_BITS (= 64)
  //   MPFR_PREC(u) <= GMP_NUMB_BITS
  //   MPFR_PREC(v) <= GMP_NUMB_BITS
  //
  // Ref: mpfr/src/div.c L265-L267 — MPFR_ASSERTD guards.

  // Ref: mpfr/src/div.c L258-L259.
  let qx: bigint = u.exp - v.exp;

  // u0 and v0 are the MSB-aligned 64-bit mantissa limbs.
  // In our TS MPFR encoding, mant is already an integer in [2^(prec-1), 2^prec).
  // For prec=64, u0 ∈ [2^63, 2^64).
  // Ref: mpfr/src/div.c L260-L261.
  let u0: bigint = u.mant;
  const v0: bigint = v.mant;

  // If u.prec < 64, the mantissa needs to be left-shifted to occupy 64 bits.
  // The C code accesses MPFR_MANT(u)[0] which is always the full 64-bit limb
  // (with trailing zeros if prec < 64). We replicate: shift u0 left to 64 bits.
  // Ref: mpfr/src/div.c L260 — u0 = MPFR_MANT(u)[0] (the raw 64-bit limb).
  if (u.prec < PREC_64) {
    u0 = u0 << (PREC_64 - u.prec);
  }
  // v0 similarly. In C MPFR_MANT(v)[0] is always the raw 64-bit limb.
  let v0_aligned: bigint = v0;
  if (v.prec < PREC_64) {
    v0_aligned = v0 << (PREC_64 - v.prec);
  }

  // Step 1 — Normalise: extra = (u0 >= v0).
  // Ref: mpfr/src/div.c L269-L271.
  let extra: 0 | 1;
  if (u0 >= v0_aligned) {
    u0 = u0 - v0_aligned;
    extra = 1;
  } else {
    extra = 0;
  }

  // Step 2 — Compute floor quotient q0 and remainder l.
  // C uses __gmpfr_invert_limb_approx + correction; we use BigInt division.
  // u0 is now in [0, v0_aligned), so q0 = floor(u0 * 2^64 / v0_aligned).
  // Ref: mpfr/src/div.c L272-L296 (64-bit branch with invert_limb_approx).
  const dividend = u0 * LIMB_BASE;
  let q0: bigint = dividend / v0_aligned;
  let l: bigint = dividend - q0 * v0_aligned;

  // Step 3 — Extract qp[0], rb, sb per extra flag.
  // Ref: mpfr/src/div.c L302-L323.
  let qp0: bigint;   // The 64-bit quotient mantissa limb
  let rb: bigint;    // Round bit (1 or 0)
  let sb: bigint;    // Sticky bit (0 means exact so far, nonzero means inexact)

  if (extra === 0) {
    // Ref: mpfr/src/div.c L309-L316.
    // qp[0] = q0
    // rb = (l+l carries) OR (l+l >= v0)
    // i.e. rb = (2*l >= v0_aligned)
    qp0 = q0;
    const twoL = l << 1n;
    const twoLCarries = twoL >= LIMB_BASE;  // carry out of 64-bit addition l+l
    const twoLActual = twoL & (LIMB_BASE - 1n);  // lower 64 bits of 2*l
    if (twoLCarries || twoLActual >= v0_aligned) {
      rb = 1n;
      // sb = twoL - v0_aligned (if rb=1); we only need zero/nonzero
      // but compute it faithfully.
      // Actual remainder after 2l - v0: could be twoLActual - v0_aligned
      // but if carry, twoL > LIMB_BASE > v0_aligned (since v0_aligned < LIMB_BASE),
      // so sb = twoL - v0_aligned (which is the actual l+l-v0 residual).
      sb = twoL - v0_aligned;
    } else {
      rb = 0n;
      sb = l;  // l itself is the sticky (zero iff no remainder)
    }
  } else {
    // extra == 1
    // Ref: mpfr/src/div.c L317-L323.
    // qp[0] = MPFR_LIMB_HIGHBIT | (q0 >> 1)
    // rb = q0 & 1
    // sb = l
    // qx++
    qp0 = HIGHBIT | (q0 >> 1n);
    rb = q0 & 1n;
    sb = l;
    qx += 1n;
  }

  // Result sign = sign(u) * sign(v).
  // Ref: mpfr/src/div.c L325.
  const resultSign: Sign = multSign(u.sign, v.sign);

  // Step 4 — Overflow check.
  // Ref: mpfr/src/div.c L328-L329.
  if (qx > EMAX_DEFAULT) {
    return mpfr_overflow(PREC_64, rnd, resultSign);
  }

  // Step 5 — (Underflow check omitted per spec divergence note.)
  // Ref: mpfr/src/div.c L334-L352 — underflow branch exists in C but
  //   is omitted in TS port per spec.json "divergence_from_c".

  // Step 6 — Rounding.
  // Ref: mpfr/src/div.c L356-L387.

  if (rb === 0n && sb === 0n) {
    // Exact.
    // Ref: mpfr/src/div.c L356-L359.
    return {
      value: { kind: 'normal', sign: resultSign, prec: PREC_64, exp: qx, mant: qp0 },
      ternary: 0,
    };
  }

  if (rnd === 'RNDN') {
    // For div_1n, rb != 0 implies sb != 0 (a 64-bit/64-bit exact division
    // cannot give an exact 65-bit quotient). So we cannot be at the midpoint
    // (rb=1, sb=0). Thus if rb=0 we truncate; if rb=1 we add_one_ulp.
    // Ref: mpfr/src/div.c L361-L371.
    if (rb === 0n) {
      // Truncate (round toward zero).
      return {
        value: { kind: 'normal', sign: resultSign, prec: PREC_64, exp: qx, mant: qp0 },
        ternary: resultSign === 1 ? -1 : 1,
      };
    } else {
      // add_one_ulp.
      const newMant = qp0 + 1n;
      // No carry possible — see proof in mpfr/src/div.c L383-L385.
      return {
        value: { kind: 'normal', sign: resultSign, prec: PREC_64, exp: qx, mant: newMant },
        ternary: resultSign === 1 ? 1 : -1,
      };
    }
  }

  if (isLikeRNDZ(rnd, resultSign)) {
    // Truncate: round toward zero.
    // Ref: mpfr/src/div.c L373-L377.
    return {
      value: { kind: 'normal', sign: resultSign, prec: PREC_64, exp: qx, mant: qp0 },
      ternary: resultSign === 1 ? -1 : 1,
    };
  }

  // Round away from zero: add_one_ulp.
  // Ref: mpfr/src/div.c L379-L386.
  // qp[0] += MPFR_LIMB_ONE (= 1n, since sh=0)
  const newMant = qp0 + 1n;
  // No carry possible (see analysis in mpfr/src/div.c L383-L385).
  return {
    value: { kind: 'normal', sign: resultSign, prec: PREC_64, exp: qx, mant: newMant },
    ternary: resultSign === 1 ? 1 : -1,
  };
}
