/**
 * ops/mul_1.ts — pure-TS port of MPFR's `mpfr_mul_1`.
 *
 * Single-limb multiplication fast path for `prec(a) < GMP_NUMB_BITS (= 64)`
 * AND `prec(b), prec(c) <= GMP_NUMB_BITS` AND `prec(a) == prec(b) == prec(c)`.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_mul_1(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                         mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_mul_1(b, c, prec, rnd) -> Result
 *
 * Pre-conditions
 * --------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.prec === c.prec === prec` (same precision)
 *   - `1n <= prec <= 63n` (single-limb; p < GMP_NUMB_BITS)
 *
 * Algorithm
 * ---------
 *
 * Mirrors mpfr/src/mul.c L270-L364 exactly.
 *
 * 1. ax = EXP(b) + EXP(c)  — the naive product exponent.
 * 2. (a0, sb) = umul_ppmm(b0, c0) — 64-bit * 64-bit -> 128-bit product.
 *    b0, c0 are the mantissas in C-limb form (MSB-aligned to 64 bits).
 *    In BigInt: full = b0 * c0; a0 = full >> 64; sb = full & LIMB_MASK.
 *    NOTE: umul_ppmm(hi, lo, a, b) — hi is the *first* output arg (high word).
 *    Ref: CLAUDE.md "umul_ppmm output args are first".
 * 3. if a0 < HIGHBIT: ax--; a0 = (a0 << 1) | (sb >> 63); sb <<= 1.
 *    This normalises: the product may be one bit short of filling 64 bits.
 * 4. mask = (1 << sh) - 1  where sh = 64 - prec.
 * 5. rb = a0 & (1 << (sh - 1))  — the round bit.
 * 6. sb |= (a0 & mask) ^ rb    — accumulate sticky bits.
 * 7. ap[0] = a0 & ~mask         — truncated result limb.
 * 8. sign = b.sign * c.sign.
 * 9. If ax > emax: overflow.
 * 10. Rounding: apply 5-mode logic, add_one_ulp if needed.
 *
 * Underflow is omitted per spec.json divergence_from_c: the TS locked
 * schema has unbounded exponent range (no emin in the default port).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/mul.c L270-L364 — the regular C reference body.
 *   - src/ops/add1sp1.ts — parallel single-limb add fast path (same shape).
 *   - src/ops/overflow.ts — exp-overflow delegate.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts: umul_ppmm output args are first".
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is sign of (rounded - exact)".
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from "../core.ts";
import { MPFRError } from "../core.ts";
import { mpfr_overflow } from "./overflow.ts";

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** GMP_NUMB_BITS on x86_64. Ref: mpfr/src/mul.c L278 — `GMP_NUMB_BITS - p`. */
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
 * MPFR_LIMB_MASK(s): bitmask with lowest `s` bits set.
 * Ref: mpfr/src/mpfr-impl.h L1308-L1311.
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
 *
 * Ref: mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 */
function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

/**
 * Convert C-limb form (MSB-aligned to 64 bits) back to TS-schema MPFR.
 * mant = ap0 >> sh  where sh = 64 - prec.
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

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Single-limb multiplication fast path for `prec < 64`.
 *
 * Mirrors `mpfr_mul_1` from `mpfr/src/mul.c L270-L364`.
 *
 * @mpfrName mpfr_mul_1
 *
 * @param b   First factor; must be `kind='normal'`, `1 <= prec <= 63`.
 * @param c   Second factor; must be `kind='normal'`, same `prec`.
 * @param prec  Target precision in bits, must equal `b.prec === c.prec`.
 * @param rnd Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }` — the correctly-rounded product and the
 *          ternary flag (sign of rounded − exact).
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_mul_1(
  b: MPFR,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/mul.c L274-L276 — MPFR_ASSERTD checks (elevated to throws).
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_mul_1: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_mul_1: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (prec < 1n || prec > 63n) {
    throw new MPFRError('EPREC', `mpfr_mul_1: prec must be in [1,63], got ${prec}`);
  }
  if (b.prec !== prec) {
    throw new MPFRError('EPREC', `mpfr_mul_1: b.prec (${b.prec}) !== prec (${prec})`);
  }
  if (c.prec !== prec) {
    throw new MPFRError('EPREC', `mpfr_mul_1: c.prec (${c.prec}) !== prec (${prec})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_mul_1: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Set up C-mirror variables
  // Ref: mpfr/src/mul.c L273-L280.
  // -------------------------------------------------------------------------

  const p = prec;                             // mpfr_prec_t p
  const sh = GMP_NUMB_BITS - p;              // sh = 64 - p (shift amount)
  const mask = lmbMask(sh);                  // MPFR_LIMB_MASK(sh)

  // Convert TS-schema mant (MSB-aligned to prec bits) to C-limb form
  // (MSB-aligned to 64 bits): b0 = b.mant << sh.
  // Ref: mpfr/src/mul.c L275-L276 — b0 = MPFR_MANT(b)[0]; c0 = MPFR_MANT(c)[0].
  const b0 = b.mant << sh;
  const c0 = c.mant << sh;

  // -------------------------------------------------------------------------
  // umul_ppmm(a0, sb, b0, c0)
  // NOTE: umul_ppmm(hi, lo, a, b) — first two args are *outputs*.
  // hi = high 64 bits of b0*c0; lo = low 64 bits.
  // Ref: mpfr/src/mul.c L288 — `umul_ppmm (a0, sb, b0, c0)`.
  // Ref: CLAUDE.md "umul_ppmm output args are first".
  // -------------------------------------------------------------------------

  // ax = EXP(b) + EXP(c) — naive product exponent.
  // Ref: mpfr/src/mul.c L287 — `ax = MPFR_GET_EXP(b) + MPFR_GET_EXP(c)`.
  let ax: bigint = b.exp + c.exp;

  // Full 128-bit product, then split into high (a0) and low (sb) 64-bit words.
  const fullProduct = b0 * c0;
  let a0 = fullProduct >> GMP_NUMB_BITS;   // high 64 bits
  let sb = fullProduct & LIMB_MASK_64;     // low 64 bits

  // -------------------------------------------------------------------------
  // Normalisation step (if needed).
  // Ref: mpfr/src/mul.c L289-L297.
  //
  // If a0 < HIGHBIT, the product is one bit short of 64-bit alignment.
  // Shift left by 1 (combine a0 and top bit of sb), decrement ax.
  // This ensures a0 has its MSB set (is normalised).
  // -------------------------------------------------------------------------

  if (a0 < LIMB_HIGHBIT) {
    ax--;
    // Ref: mpfr/src/mul.c L295-L296:
    //   a0 = (a0 << 1) | (sb >> (GMP_NUMB_BITS - 1))
    //   sb <<= 1
    a0 = ((a0 << 1n) | (sb >> 63n)) & LIMB_MASK_64;
    sb = (sb << 1n) & LIMB_MASK_64;
  }

  // -------------------------------------------------------------------------
  // Extract round bit and sticky bits.
  // Ref: mpfr/src/mul.c L298-L300.
  //
  //   rb = a0 & (MPFR_LIMB_ONE << (sh - 1))
  //   sb |= (a0 & mask) ^ rb
  //   ap[0] = a0 & ~mask
  // -------------------------------------------------------------------------

  const rb = a0 & (1n << (sh - 1n));
  sb = (sb | ((a0 & mask) ^ rb)) & LIMB_MASK_64;
  const ap0 = a0 & ~mask;

  // -------------------------------------------------------------------------
  // Sign: product-of-signs rule.
  // Ref: mpfr/src/mul.c L302 — MPFR_SIGN(a) = MPFR_MULT_SIGN(MPFR_SIGN(b), MPFR_SIGN(c)).
  // Ref: CLAUDE.md "Ternary flag" callout — sign of result matters for ternary.
  // -------------------------------------------------------------------------

  const sign: Sign = (b.sign * c.sign) as Sign;

  // -------------------------------------------------------------------------
  // Overflow check (pre-rounding).
  // Ref: mpfr/src/mul.c L305-L306.
  // -------------------------------------------------------------------------

  if (ax > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
  }

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: mpfr/src/mul.c L331-L363.
  //
  // Convention for ternary: sign of (rounded − exact).
  //   truncate → ternary = −sign  (C: MPFR_RET(-MPFR_SIGN(a)))
  //   add_one_ulp → ternary = +sign  (C: MPFR_RET(MPFR_SIGN(a)))
  //
  // Note the ternary sign is the *result* sign, not the operand sign.
  // Ref: CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign
  //   of (rounded - exact), not 0/1."
  // -------------------------------------------------------------------------

  // Exact case: no rounding needed.
  // Ref: mpfr/src/mul.c L331 — `if ((rb == 0 && sb == 0) ...) MPFR_RET(0)`.
  if (rb === 0n && sb === 0n) {
    const value: MPFR = buildNormal(p, sign, ax, ap0, sh);
    return { value, ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // -----------------------------------------------------------------------
    // Round to nearest (ties to even).
    // Ref: mpfr/src/mul.c L336-L340.
    //
    // Truncate if:
    //   rb == 0  (below midpoint)
    //   OR (rb != 0 AND sb == 0 AND LSB of stored result is 0 — tie → even)
    //
    // "bit at position sh" of ap0 is the LSB of the p-bit stored result.
    // Ref: mul.c L338: `if (rb == 0 || (sb == 0 && (ap[0] & (MPFR_LIMB_ONE << sh)) == 0))`
    // -----------------------------------------------------------------------
    const lsb = ap0 & (1n << sh);
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    // Truncate toward zero.
    // Ref: mpfr/src/mul.c L343-L347.
    doAddOneUlp = false;
  } else {
    // Round away from zero (RNDA, or RNDU when positive, or RNDD when negative).
    // Ref: mpfr/src/mul.c L349-L362.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // truncate branch: ternary = -sign
    // Ref: mul.c L347 — `MPFR_RET(-MPFR_SIGN(a))`.
    const value: MPFR = buildNormal(p, sign, ax, ap0, sh);
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value, ternary };
  }

  // add_one_ulp branch.
  // Ref: mpfr/src/mul.c L351-L362.
  //
  //   ap[0] += MPFR_LIMB_ONE << sh
  //   if ap[0] == 0 (overflow): ap[0] = LIMB_HIGHBIT; ax++; check emax.
  //   ternary = MPFR_SIGN(a)

  let newAp0 = (ap0 + (1n << sh)) & LIMB_MASK_64;
  let newAx = ax;

  if (newAp0 === 0n) {
    // The add_one_ulp carry propagated all the way through the limb.
    // Ref: mul.c L353-L361.
    newAp0 = LIMB_HIGHBIT;
    newAx = ax + 1n;
    if (newAx > EMAX_DEFAULT) {
      return mpfr_overflow(p, rnd, sign);
    }
  }

  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  const value: MPFR = buildNormal(p, sign, newAx, newAp0, sh);
  return { value, ternary };
}
