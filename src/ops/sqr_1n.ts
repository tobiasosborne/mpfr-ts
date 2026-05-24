/**
 * ops/sqr_1n.ts — pure-TS port of MPFR's `mpfr_sqr_1n`.
 *
 * Single-limb squaring fast path for the special case `prec == GMP_NUMB_BITS == 64`
 * exactly. This is the sister function to `mpfr_sqr_1` (which handles `prec < 64`),
 * but here `sh = GMP_NUMB_BITS - p = 0` so no shift is needed — the mantissa already
 * exactly fills the 64-bit limb.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_sqr_1n(mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_sqr_1n(b, rnd) -> Result
 *
 * Pre-conditions (MPFRError in TS)
 * ---------------------------------
 *
 *   - `b.kind === 'normal'`
 *   - `b.prec === 64n`
 *
 * Algorithm (sh == 0, p == 64)
 * ----------------------------
 *
 * The C version uses `umul_ppmm(a0, sb, b0, b0)` which produces the 128-bit
 * product of b0*b0 as (a0=high_64, sb=low_64). In TS we use BigInt multiplication
 * directly to get the full 128-bit result.
 *
 *   ax = b.exp * 2
 *   {a0, sb} = b0 * b0   (128-bit product, a0=high64, sb=low64)
 *
 * Normalization:
 *   if a0 < HIGHBIT:
 *     ax--
 *     a0 = ((a0 << 1) | (sb >> 63)) & MASK64   -- shift-left by 1
 *     sb = (sb << 1) & MASK64
 *
 * Round/sticky bits (sh == 0, so rb is MSB of sb, sb is rest):
 *   rb = sb & HIGHBIT
 *   sb = sb & ~HIGHBIT
 *
 * Result:
 *   ap[0] = a0   (no mask needed since sh == 0)
 *   sign is always +1 (squaring)
 *
 * Rounding mirrors mpfr_sqr_1 exactly except sh == 0:
 *   - RNDN tie: check ap[0] & MPFR_LIMB_ONE (= ap[0] & 1n)
 *   - add_one_ulp: ap[0] += 1n (MPFR_LIMB_ONE << sh == 1n << 0 == 1)
 *   - overflow on carry: ap[0] = HIGHBIT, ax++
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sqr.c L133-L226 — the C reference body (prec=64 specialization).
 *   - mpfr/src/sqr.c L548 — dispatcher routes prec=64 here.
 *   - src/ops/add1sp1n.ts — parallel single-limb same-prec fast path.
 *   - src/ops/overflow.ts — exponent-overflow delegate.
 *   - CLAUDE.md 'Hallucination-risk callouts: umul_ppmm output args are first'
 *     (umul_ppmm(hi, lo, a, b) -- hi and lo are OUTPUTS).
 *   - CLAUDE.md 'Hallucination-risk callouts: Ternary flag is the sign of
 *     (rounded - exact), not 0/1.'
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from "../core.ts";
import { MPFRError } from "../core.ts";
import { mpfr_overflow } from "./overflow.ts";

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
// Helper: MPFR_IS_LIKE_RNDZ
// ---------------------------------------------------------------------------

/**
 * TS analogue of MPFR_IS_LIKE_RNDZ (mpfr-impl.h L1233-L1234):
 * "does `rnd` round toward zero with respect to the sign?"
 *
 * For squaring the result is always positive (sign=1), so:
 *   RNDZ → toward zero (always)
 *   RNDD → toward -inf, which for positive = toward zero
 *   RNDU → toward +inf, which for positive = away from zero
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
 * Single-limb squaring fast path for `prec == 64`.
 *
 * Mirrors `mpfr_sqr_1n` from `mpfr/src/sqr.c L133-L226`.
 *
 * @mpfrName mpfr_sqr_1n
 *
 * @param b   Input operand; must be `kind='normal'`, `prec === 64n`.
 * @param rnd Rounding mode (five modes per `src/core.ts`).
 *
 * @returns `{ value, ternary }` — the correctly-rounded square and the
 *          ternary flag (sign of rounded − exact).
 *          The result sign is always +1 (squaring produces a non-negative result).
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_sqr_1n(
  b: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions.
  // Ref: mpfr/src/sqr.c L134-L141 — function setup, MPFR_MANT/GET_EXP.
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sqr_1n: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (b.prec !== 64n) {
    throw new MPFRError('EPREC', `mpfr_sqr_1n: b.prec must be 64n, got ${b.prec}`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_sqr_1n: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Set up C-mirror variables.
  // Ref: mpfr/src/sqr.c L136-L141.
  //
  //   mp_limb_t b0 = MPFR_MANT(b)[0];   -- the single 64-bit limb of b
  //   mpfr_exp_t ax = MPFR_GET_EXP(b) * 2;
  //
  // For prec=64 and sh=0, b.mant is exactly the single limb b0.
  // The result sign is always +1 (squaring).
  // -------------------------------------------------------------------------

  const b0: bigint = b.mant;
  let ax: bigint = b.exp * 2n;

  // -------------------------------------------------------------------------
  // Core computation: umul_ppmm(a0, sb, b0, b0)
  // Ref: mpfr/src/sqr.c L143 — `umul_ppmm (a0, sb, b0, b0);`
  //
  // umul_ppmm(hi, lo, a, b): FIRST TWO args are OUTPUTS.
  //   hi = floor(a*b / 2^64)
  //   lo = (a*b) mod 2^64
  //
  // In TS, BigInt multiplication gives the full 128-bit product directly.
  // -------------------------------------------------------------------------

  const product: bigint = b0 * b0;  // full 128-bit result
  let a0: bigint = product >> 64n;  // high 64 bits
  let sb: bigint = product & LIMB_MASK_64;  // low 64 bits

  // -------------------------------------------------------------------------
  // Normalization: ensure MSB of a0 is set.
  // Ref: mpfr/src/sqr.c L144-L148.
  //
  //   if (a0 < MPFR_LIMB_HIGHBIT)
  //     {
  //       ax --;
  //       a0 = (a0 << 1) | (sb >> (GMP_NUMB_BITS - 1));
  //       sb <<= 1;
  //     }
  //
  // Since b is normalised (b0 >= HIGHBIT), b0*b0 >= HIGHBIT^2 = 2^126,
  // so the 128-bit product has bit 126 set, meaning a0 >= 2^62.
  // If a0 < HIGHBIT (2^63), we shift left by 1 and decrement ax.
  // -------------------------------------------------------------------------

  if (a0 < LIMB_HIGHBIT) {
    ax -= 1n;
    a0 = ((a0 << 1n) | (sb >> 63n)) & LIMB_MASK_64;
    sb = (sb << 1n) & LIMB_MASK_64;
  }

  // -------------------------------------------------------------------------
  // Extract round and sticky bits.
  // Ref: mpfr/src/sqr.c L150-L152.
  //
  //   rb = sb & MPFR_LIMB_HIGHBIT;
  //   sb = sb & ~MPFR_LIMB_HIGHBIT;
  //   ap[0] = a0;
  //
  // For sh == 0 (prec == 64): rb is the MSB of the low product limb,
  // sb is all the remaining bits of the low limb. No mask needed for ap[0]
  // since sh == 0 means no bits need clearing.
  // -------------------------------------------------------------------------

  const rb: bigint = sb & LIMB_HIGHBIT;
  sb = sb & ~LIMB_HIGHBIT;
  const ap0: bigint = a0;

  // Result sign is always positive for squaring.
  // Ref: mpfr/src/sqr.c L154 — `MPFR_SIGN(a) = MPFR_SIGN_POS;`
  const sign: Sign = 1;

  // -------------------------------------------------------------------------
  // Overflow check (pre-rounding).
  // Ref: mpfr/src/sqr.c L157-L158.
  //
  //   if (MPFR_UNLIKELY(ax > __gmpfr_emax))
  //     return mpfr_overflow (a, rnd_mode, MPFR_SIGN_POS);
  // -------------------------------------------------------------------------

  if (ax > EMAX_DEFAULT) {
    return mpfr_overflow(64n, rnd, sign);
  }

  // -------------------------------------------------------------------------
  // Underflow check is omitted per spec: "Underflow check omitted (per
  // locked-schema policy)." The TS surface does not model emin.
  // Ref: spec.json divergence_from_c.
  // -------------------------------------------------------------------------

  // -------------------------------------------------------------------------
  // Rounding.
  // Ref: mpfr/src/sqr.c L190-L225.
  //
  // Ternary convention: sign of (rounded − exact).
  //   truncate → rounded < exact → ternary = -1 (for positive result)
  //     C: MPFR_RET(-MPFR_SIGN_POS) = -1
  //   add_one_ulp → rounded > exact → ternary = +1 (for positive result)
  //     C: MPFR_RET(MPFR_SIGN_POS) = +1
  //
  // RNDN tie-to-even check for sh==0:
  //   ap[0] & MPFR_LIMB_ONE = ap[0] & 1
  //   Ref: mpfr/src/sqr.c L200 — `(ap[0] & MPFR_LIMB_ONE) == 0`.
  // -------------------------------------------------------------------------

  if (rb === 0n && sb === 0n) {
    // Exact result.
    // Ref: mpfr/src/sqr.c L193-L196 — `if ((rb == 0 && sb == 0) ...) MPFR_RET(0)`.
    return { value: buildResult(sign, ax, ap0), ternary: 0 };
  }

  let doAddOneUlp: boolean;

  if (rnd === 'RNDN') {
    // Round to nearest, ties to even.
    // Ref: mpfr/src/sqr.c L198-L203.
    //
    //   if (rb == 0 || (sb == 0 && (ap[0] & MPFR_LIMB_ONE) == 0))
    //     goto truncate;
    //   else
    //     goto add_one_ulp;
    //
    // Tie (rb!=0, sb==0): round to even, i.e. round up only if LSB of ap[0] is 1.
    const lsb = ap0 & 1n;  // MPFR_LIMB_ONE = 1 (sh == 0)
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    // Truncate toward zero.
    // Ref: mpfr/src/sqr.c L205-L208 — `MPFR_IS_LIKE_RNDZ(rnd_mode, 0) → goto truncate`.
    doAddOneUlp = false;
  } else {
    // Round away from zero (RNDA, or RNDU/RNDD in the away-from-zero direction).
    // Ref: mpfr/src/sqr.c L211 — `else /* round away from zero */ goto add_one_ulp`.
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    // truncate branch: ternary = -sign = -1.
    // Ref: mpfr/src/sqr.c L208-L209 — `MPFR_RET(-MPFR_SIGN_POS)` = -1.
    return { value: buildResult(sign, ax, ap0), ternary: -1 };
  }

  // add_one_ulp branch.
  // Ref: mpfr/src/sqr.c L213-L224.
  //
  //   ap[0] += MPFR_LIMB_ONE;   (MPFR_LIMB_ONE << sh = 1 << 0 = 1)
  //   if (ap[0] == 0)           (all-ones + 1 = carry)
  //     {
  //       ap[0] = MPFR_LIMB_HIGHBIT;
  //       if (MPFR_UNLIKELY(ax + 1 > __gmpfr_emax))
  //         return mpfr_overflow (a, rnd_mode, MPFR_SIGN_POS);
  //       MPFR_SET_EXP (a, ax + 1);
  //     }
  //   MPFR_RET(MPFR_SIGN_POS);  = +1

  let newAp0 = (ap0 + 1n) & LIMB_MASK_64;
  let newAx = ax;

  if (newAp0 === 0n) {
    // Mantissa carry-out: the add_one_ulp rippled all the way through.
    // Ref: mpfr/src/sqr.c L215-L223.
    newAp0 = LIMB_HIGHBIT;
    newAx = ax + 1n;
    if (newAx > EMAX_DEFAULT) {
      return mpfr_overflow(64n, rnd, sign);
    }
  }

  // ternary = +MPFR_SIGN_POS = +1.
  return { value: buildResult(sign, newAx, newAp0), ternary: 1 };
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
 * Ref: mpfr/src/sqr.c L191 — `MPFR_EXP(a) = ax` followed by storing `ap[0]`.
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
