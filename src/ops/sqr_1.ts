/**
 * ops/sqr_1.ts — pure-TS port of MPFR's `mpfr_sqr_1`.
 *
 * Single-limb squaring fast path for `prec(b) == prec(a) < GMP_NUMB_BITS` (= 64).
 * This is the most frequently invoked squaring path for sub-64-bit precisions.
 *
 * C signature (MPFR):
 *
 *   static int mpfr_sqr_1(mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature (this port):
 *
 *   mpfr_sqr_1(b, prec, rnd) -> Result
 *
 * Pre-conditions (MPFR_ASSERTD in C, MPFRError in TS):
 *   - `b.kind === 'normal'`
 *   - `1n <= b.prec <= 63n` and `b.prec === prec`
 *
 * The output precision is `prec`; output is always positive (squaring).
 *
 * Algorithm
 * ---------
 *
 * All arithmetic runs in C "limb space": mantissas are MSB-aligned to
 * 64 bits (C convention), not MSB-aligned to `prec` bits (TS schema
 * convention). The conversion is:
 *
 *   b_limb = b.mant << sh   where sh = 64 - prec
 *
 * The algorithm (mpfr/src/sqr.c L32-L130):
 *   1. ax = EXP(b) * 2
 *   2. (a0, sb) = 128-bit product of b0 * b0 (umul_ppmm)
 *   3. If a0 < 2^63 (MSB unset): shift left by 1 (ax--, a0 |= sb>>63, sb<<=1)
 *   4. rb = a0 & (1 << (sh-1))     -- round bit
 *   5. sb |= (a0 & mask) ^ rb      -- sticky bits from a0's low bits
 *   6. ap[0] = a0 & ~mask          -- zero the round+sticky bits
 *   7. sign = POS (squaring always yields positive)
 *   8. Check overflow (ax > emax)
 *   9. Check underflow (ax < emin) with special RNDN/RNDA no-underflow checks
 *   10. Round per MPFR's standard 5-mode rounding logic
 *   11. On add_one_ulp carry, bump exponent; check overflow again
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sqr.c L32-L130 — the C reference body (prec < 64 specialization).
 *   - mpfr/src/sqr.c L539-L554 — mpfr_sqr dispatcher: routes prec=prec(b)<64 here.
 *   - mpfr/src/mpfr-impl.h L1233-L1234 — MPFR_IS_LIKE_RNDZ macro.
 *   - mpfr/src/mpfr-impl.h L1240-L1243 — MPFR_IS_LIKE_RNDA macro.
 *   - mpfr/src/mpfr-impl.h L1300-L1311 — MPFR_LIMB_HIGHBIT, MPFR_LIMB_MASK.
 *   - mpfr/src/mpfr.h L231-L232 — MPFR_EMAX_DEFAULT / MPFR_EMIN_DEFAULT.
 *   - src/ops/overflow.ts — delegate for exponent overflow.
 *   - src/ops/underflow.ts — delegate for exponent underflow.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts: umul_ppmm output args are first"
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign of (rounded - exact)"
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError } from '../core.ts';
import { mpfr_overflow } from './overflow.ts';
import { mpfr_underflow } from './underflow.ts';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** GMP_NUMB_BITS = 64 on x86_64. */
const GMP_NUMB_BITS = 64n;

/** MPFR_LIMB_HIGHBIT = 1 << 63.
 * Ref: mpfr/src/mpfr-impl.h L1301.
 */
const LIMB_HIGHBIT = 1n << 63n;

/** 64-bit mask (all bits set). */
const LIMB_MASK_64 = (1n << GMP_NUMB_BITS) - 1n;

/**
 * Default exponent ceiling. Mirrors `MPFR_EMAX_DEFAULT = (1 << 30) - 1`.
 * Ref: mpfr/src/mpfr.h L231.
 */
const EMAX_DEFAULT = (1n << 30n) - 1n;

/**
 * Default exponent floor. Mirrors `MPFR_EMIN_DEFAULT = -(MPFR_EMAX_DEFAULT) = -(2^30-1)`.
 * Ref: mpfr/src/mpfr.h L232.
 */
const EMIN_DEFAULT = -EMAX_DEFAULT;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * MPFR_LIMB_MASK(s): bitmask with lowest s bits set.
 * Ref: mpfr/src/mpfr-impl.h L1308-L1311.
 * Precondition: 0 <= s <= 64.
 */
function limbMask(s: bigint): bigint {
  if (s === 0n) return 0n;
  if (s >= 64n) return LIMB_MASK_64;
  return (1n << s) - 1n;
}

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ (mpfr-impl.h L1233-L1234):
 * true when the rounding mode rounds toward zero with respect to the sign.
 * Since squaring is always positive (sign=1), we only need to handle sign=1 here,
 * but we keep the general form for clarity.
 *
 * MPFR_IS_LIKE_RNDZ(rnd, neg) where neg = (sign < 0):
 *   rnd == RNDZ || (rnd == RNDU && neg) || (rnd == RNDD && !neg)
 *
 * For squaring (sign=1, neg=false):
 *   RNDZ or RNDD
 */
function isLikeRNDZ_pos(rnd: RoundingMode): boolean {
  return rnd === 'RNDZ' || rnd === 'RNDD';
}

/**
 * TS analogue of MPFR_IS_LIKE_RNDA (mpfr-impl.h L1240-L1243):
 * true when the rounding mode rounds away from zero with respect to the sign.
 * For squaring (sign=1, neg=false): RNDA or RNDU.
 *
 * Ref: mpfr/src/sqr.c L81 — MPFR_IS_LIKE_RNDA (rnd_mode, 0)
 */
function isLikeRNDA_pos(rnd: RoundingMode): boolean {
  return rnd === 'RNDA' || rnd === 'RNDU';
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

const VALID_RND: readonly RoundingMode[] = Object.freeze([
  'RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA',
] as const);

/**
 * Single-limb squaring fast path for `prec(b) == prec < 64`.
 *
 * Mirrors `mpfr_sqr_1` from `mpfr/src/sqr.c L32-L130`.
 *
 * @mpfrName mpfr_sqr_1
 *
 * @param b    Input value; must be `kind='normal'`, `1 <= prec <= 63`.
 * @param prec Output precision in bits; must equal `b.prec` and be in `[1, 63]`.
 * @param rnd  Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }` — the correctly-rounded square and the
 *          ternary flag (sign of rounded − exact). Output sign is always +1.
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_sqr_1(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/sqr.c L32-L40 (MPFR_ASSERTD in C, elevated to throws).
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sqr_1: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_sqr_1: prec must be bigint, got ${typeof prec}`);
  }
  if (prec < 1n || prec > 63n) {
    throw new MPFRError('EPREC', `mpfr_sqr_1: prec must be in [1, 63], got ${prec}`);
  }
  if (b.prec !== prec) {
    throw new MPFRError('EPREC', `mpfr_sqr_1: b.prec (${b.prec}) must equal prec (${prec})`);
  }
  if (!VALID_RND.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_sqr_1: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Setup
  // Ref: mpfr/src/sqr.c L35-L41.
  // -------------------------------------------------------------------------

  // sh = GMP_NUMB_BITS - p; p = prec
  // Ref: mpfr/src/sqr.c L39.
  const sh = GMP_NUMB_BITS - prec;

  // mask = MPFR_LIMB_MASK(sh) — the sh lowest bits
  // Ref: mpfr/src/sqr.c L41.
  const mask = limbMask(sh);

  // Convert TS-schema mant (MSB-aligned to prec bits) to C-limb form
  // (MSB-aligned to 64 bits): b0 = b.mant << sh.
  // Ref: The TS-to-C conversion (spec.json, src/core.ts L131-L134).
  const b0 = b.mant << sh;

  // ax = EXP(b) * 2
  // Ref: mpfr/src/sqr.c L48.
  let ax = b.exp * 2n;

  // -------------------------------------------------------------------------
  // Compute 128-bit product b0 * b0
  // In C: umul_ppmm(a0, sb, b0, b0) — a0 is the HIGH limb, sb is the LOW limb.
  // In BigInt: (a0, sb) where a0 = (b0 * b0) >> 64, sb = (b0 * b0) & MASK64.
  //
  // HALLUCINATION GUARD: umul_ppmm(hi, lo, a, b) — first two args are OUTPUTS.
  // Ref: CLAUDE.md "Hallucination-risk callouts: umul_ppmm output args are first".
  // Ref: mpfr/src/sqr.c L49.
  // -------------------------------------------------------------------------
  const product = b0 * b0;  // 128-bit BigInt (b0 < 2^64, so product < 2^128)
  let a0 = product >> GMP_NUMB_BITS;   // high 64-bit limb
  let sb = product & LIMB_MASK_64;     // low 64-bit limb

  // -------------------------------------------------------------------------
  // Normalize: if MSB of a0 is unset, shift left by 1 and adjust exponent.
  // Ref: mpfr/src/sqr.c L50-L55.
  // -------------------------------------------------------------------------
  if (a0 < LIMB_HIGHBIT) {
    // ax --
    ax -= 1n;
    // a0 = (a0 << 1) | (sb >> (GMP_NUMB_BITS - 1))
    a0 = ((a0 << 1n) | (sb >> (GMP_NUMB_BITS - 1n))) & LIMB_MASK_64;
    // sb <<= 1
    sb = (sb << 1n) & LIMB_MASK_64;
  }

  // -------------------------------------------------------------------------
  // Extract round bit and sticky bits.
  // Ref: mpfr/src/sqr.c L56-L58.
  // -------------------------------------------------------------------------

  // rb = a0 & (MPFR_LIMB_ONE << (sh - 1))
  // Ref: mpfr/src/sqr.c L56.
  const rb = a0 & (1n << (sh - 1n));

  // sb |= (a0 & mask) ^ rb
  // Ref: mpfr/src/sqr.c L57.
  sb = (sb | ((a0 & mask) ^ rb)) & LIMB_MASK_64;

  // ap[0] = a0 & ~mask
  // Ref: mpfr/src/sqr.c L58.
  let ap0 = a0 & ~mask & LIMB_MASK_64;

  // Result sign is always positive (squaring).
  // Ref: mpfr/src/sqr.c L60 — MPFR_SIGN(a) = MPFR_SIGN_POS.
  const sign: 1 = 1;

  // -------------------------------------------------------------------------
  // Overflow check: ax > emax
  // Ref: mpfr/src/sqr.c L63-L64.
  // -------------------------------------------------------------------------
  if (ax > EMAX_DEFAULT) {
    return mpfr_overflow(prec, rnd, sign);
  }

  // -------------------------------------------------------------------------
  // Underflow check: ax < emin
  // Ref: mpfr/src/sqr.c L69-L92.
  //
  // Warning: underflow should be checked *after* rounding, thus when rounding
  // away and when a > 0.111...111*2^(emin-1), or when rounding to nearest and
  // a >= 0.111...111[1]*2^(emin-1), there is no underflow.
  // -------------------------------------------------------------------------
  if (ax < EMIN_DEFAULT) {
    // Check "no underflow" conditions.
    // Ref: mpfr/src/sqr.c L79-L82.
    // Condition: ax == emin - 1 && ap[0] == ~mask && ((RNDN && rb) || (RNDA_like && (rb|sb)))
    const maxMant = ~mask & LIMB_MASK_64;  // ~mask truncated to 64 bits
    if (ax === EMIN_DEFAULT - 1n && ap0 === maxMant &&
        ((rnd === 'RNDN' && rb !== 0n) ||
         (isLikeRNDA_pos(rnd) && (rb | sb) !== 0n))) {
      // no underflow: fall through to rounding
    } else {
      // Underflow.
      // For RNDN, mpfr_underflow always rounds away; to avoid rounding a tiny
      // value up, we switch to RNDZ in the cases where |a| <= 2^(emin-2).
      // Ref: mpfr/src/sqr.c L87-L90.
      let rndEffective = rnd;
      if (rnd === 'RNDN' &&
          (ax < EMIN_DEFAULT - 1n ||
           (ap0 === LIMB_HIGHBIT && (rb | sb) === 0n))) {
        rndEffective = 'RNDZ';
      }
      return mpfr_underflow(prec, rndEffective, sign);
    }
  }

  // -------------------------------------------------------------------------
  // rounding:
  // Ref: mpfr/src/sqr.c L94-L129.
  // -------------------------------------------------------------------------

  // ax might be < emin here (in the "goto rounding" case above), so we don't
  // set exp via MPFR_SET_EXP (which would assert ax >= emin); we just use ax.
  // Ref: mpfr/src/sqr.c L95-L96.

  if (rb === 0n && sb === 0n) {
    // Exact: no rounding needed.
    // Ref: mpfr/src/sqr.c L97-L101.
    // MPFR_RET(0) — return 0 (exact).
    const mant = ap0 >> sh;  // convert back to TS schema (MSB-aligned to prec bits)
    const value: MPFR = { kind: 'normal', sign, prec, exp: ax, mant };
    return { value, ternary: 0 };
  }

  if (rnd === 'RNDN') {
    // Ref: mpfr/src/sqr.c L102-L107.
    // Truncate if: rb == 0 || (sb == 0 && ap[0] & (1 << sh) == 0)
    // (ties-to-even: ap[0]'s bit at position sh is the last stored bit)
    if (rb === 0n || (sb === 0n && (ap0 & (1n << sh)) === 0n)) {
      // truncate
      const mant = ap0 >> sh;
      const value: MPFR = { kind: 'normal', sign, prec, exp: ax, mant };
      // Ternary: truncated, so rounded < exact, ternary = -1.
      // Ref: mpfr/src/sqr.c L113 — MPFR_RET(-MPFR_SIGN_POS) = return -1.
      return { value, ternary: -1 };
    } else {
      // round up (add one ulp)
      return addOneUlp(ap0, ax, sh, mask, prec, rnd, sign);
    }
  } else if (isLikeRNDZ_pos(rnd)) {
    // Truncate.
    // Ref: mpfr/src/sqr.c L109-L113.
    const mant = ap0 >> sh;
    const value: MPFR = { kind: 'normal', sign, prec, exp: ax, mant };
    // MPFR_RET(-MPFR_SIGN_POS) = return -1 (truncated, rounded down from exact positive).
    return { value, ternary: -1 };
  } else {
    // Round away from zero (RNDU or RNDA for positive sign).
    // Ref: mpfr/src/sqr.c L115-L129.
    return addOneUlp(ap0, ax, sh, mask, prec, rnd, sign);
  }
}

// ---------------------------------------------------------------------------
// Helper: add one ulp (round up)
// Ref: mpfr/src/sqr.c L117-L129.
// ---------------------------------------------------------------------------

/**
 * Add one ULP to ap0 and handle the carry (re-normalization and overflow).
 *
 * Ref: mpfr/src/sqr.c L117-L129.
 */
function addOneUlp(
  ap0: bigint,
  ax: bigint,
  sh: bigint,
  mask: bigint,
  prec: bigint,
  rnd: RoundingMode,
  sign: 1,
): Result {
  // ap[0] += MPFR_LIMB_ONE << sh
  // Ref: mpfr/src/sqr.c L118.
  ap0 = (ap0 + (1n << sh)) & LIMB_MASK_64;

  if (ap0 === 0n) {
    // Carry out: the mantissa wrapped to 0.
    // Set ap[0] = MPFR_LIMB_HIGHBIT and increment exponent.
    // Ref: mpfr/src/sqr.c L119-L127.
    ap0 = LIMB_HIGHBIT;
    // Check overflow with ax + 1.
    if (ax + 1n > EMAX_DEFAULT) {
      return mpfr_overflow(prec, rnd, sign);
    }
    ax = ax + 1n;
  }

  // MPFR_RET(MPFR_SIGN_POS) = return +1 (rounded up from exact positive).
  // Ref: mpfr/src/sqr.c L128.
  const mant = ap0 >> sh;
  const value: MPFR = { kind: 'normal', sign, prec, exp: ax, mant };
  return { value, ternary: 1 };
}
