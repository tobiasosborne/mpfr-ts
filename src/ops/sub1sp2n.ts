/**
 * ops/sub1sp2n.ts — pure-TS port of MPFR's `mpfr_sub1sp2n`.
 *
 * The same-precision, same-sign subtraction fast path for `p == 128`
 * exactly (= 2 * GMP_NUMB_BITS on x86_64), two-limb mantissas with
 * sh == 0. The dispatcher `mpfr_sub1sp` (sub1sp.c L1490-L1491) routes
 * here when `p == 2 * GMP_NUMB_BITS`.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_sub1sp2n(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                              mpfr_rnd_t rnd_mode)
 *
 * TS signature
 * ------------
 *
 *   mpfr_sub1sp2n(b, c, rnd) -> Result
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
 * Like its sister `mpfr_sub1sp2` (64 < p < 128), this port uses the
 * bigint extended-precision approach to produce a bit-identical result
 * to libmpfr without the multi-limb borrow-propagation bookkeeping.
 *
 * The difference from sub1sp2 is that `sh == 0` exactly (prec fills the
 * full 2-limb frame), which means the rounding point falls at the limb
 * boundary. The C reference (sub1sp.c L775-L1053) handles this via the
 * same overall structure: compare magnitudes, subtract, normalize via
 * count_leading_zeros, compute round/sticky bits, then round.
 *
 * Since both operands have the same prec (`p == 128`), the mathematical
 * values are:
 *
 *   value(b) = b.sign * b.mant * 2^(b.exp - 128)
 *   value(c) = c.sign * c.mant * 2^(c.exp - 128)
 *
 * The exact difference at extended precision is computed as:
 *
 *   d = large.exp - small.exp  (>= 0 when large is identified)
 *   D = (large.mant << d) - small.mant    // positive bigint
 *
 * Then D_exact = D * 2^(small.exp - 128), bitLen(D) bits. The result
 * exponent is bxIn = bitLen(D) + (small.exp - 128). Rounding is
 * delegated to `roundMantissa`.
 *
 * The `bx == cx` case (exponents equal): |b|>|c| iff b.mant>c.mant,
 * |b|<|c| iff b.mant<c.mant, exact cancellation iff b.mant==c.mant.
 * Sign rule for exact zero: RNDD → -0, else +0 (sub1.c L66-L74).
 *
 * Underflow: bx < EMIN_DEFAULT → delegate to mpfr_underflow per the
 * same narrowing logic as sub1sp2.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub1sp.c L775-L1053 — the C reference body.
 *   - mpfr/src/sub1sp.c L1490-L1491 — dispatcher routing.
 *   - mpfr/src/sub1.c L66-L74 — cancellation-zero sign rule.
 *   - src/ops/sub1sp2.ts — sister op (64 < p < 128); same bigint approach.
 *   - src/internal/mpfr/round_raw.ts — substrate rounding primitive.
 *   - src/ops/underflow.ts — delegate on bx < emin.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign
 *     of (rounded - exact), not 0/1."
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../core.ts';
import { MPFRError, posZero, negZero } from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';
import { mpfr_underflow } from './underflow.ts';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/**
 * Default maximum exponent. Mirrors MPFR_EMAX_DEFAULT.
 * Ref: mpfr/src/mpfr.h L231.
 */
const EMAX_DEFAULT = (1n << 30n) - 1n;

/**
 * Default minimum exponent. Mirrors MPFR_EMIN_DEFAULT = -EMAX_DEFAULT.
 * Ref: mpfr/src/mpfr.h L232.
 */
const EMIN_DEFAULT = -((1n << 30n) - 1n);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Bit length of a positive bigint. Returns 0n for 0. */
function bitLen(x: bigint): bigint {
  if (x <= 0n) return 0n;
  return BigInt(x.toString(2).length);
}

/** Compare two MPFR magnitudes. Returns -1, 0, +1. Both must be 'normal'. */
function cmpMag(b: MPFR, c: MPFR): -1 | 0 | 1 {
  if (b.exp !== c.exp) return b.exp < c.exp ? -1 : 1;
  if (b.mant === c.mant) return 0;
  return b.mant < c.mant ? -1 : 1;
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign subtraction fast path for `p == 128`.
 *
 * Mirrors `mpfr_sub1sp2n` from `mpfr/src/sub1sp.c L775-L1053`.
 *
 * @mpfrName mpfr_sub1sp2n
 *
 * @param b   Minuend; must be `kind='normal'`, `prec === 128n`.
 * @param c   Subtrahend; must be `kind='normal'`, same `prec` and `sign`.
 * @param rnd Rounding mode.
 *
 * @returns `{ value, ternary }` — correctly-rounded difference and ternary.
 *
 * @throws {MPFRError} `EPREC` for invalid precision or kind;
 *                     `EROUND` for unknown rounding mode.
 */
export function mpfr_sub1sp2n(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/sub1sp.c L778-L782 — MPFR_ASSERTD checks (elevated to throws).
  // -------------------------------------------------------------------------

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp2n: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_sub1sp2n: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_sub1sp2n: b.prec (${b.prec}) !== c.prec (${c.prec})`);
  }
  if (b.prec !== 128n) {
    throw new MPFRError('EPREC', `mpfr_sub1sp2n: prec must be exactly 128, got ${b.prec}`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_sub1sp2n: b.sign (${b.sign}) !== c.sign (${c.sign})`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_sub1sp2n: unknown rounding mode '${String(rnd)}'`);
  }

  const p = b.prec; // === 128n

  // -------------------------------------------------------------------------
  // Identify the larger operand and the output sign.
  // Ref: mpfr/src/sub1sp.c L792-L860 — bx==cx exact case and bx!=cx swap.
  // -------------------------------------------------------------------------

  const cmp = cmpMag(b, c);

  if (cmp === 0) {
    // Exact cancellation: |b| == |c|.
    // Sign rule: RNDD → -0, else +0.
    // Ref: mpfr/src/sub1.c L66-L74.
    const zero = rnd === 'RNDD' ? negZero(p) : posZero(p);
    return { value: zero, ternary: 0 };
  }

  let large: MPFR, small: MPFR;
  let sign: Sign;
  if (cmp === 1) {
    large = b; small = c; sign = b.sign;
  } else {
    large = c; small = b; sign = (-b.sign) as Sign;
  }

  // -------------------------------------------------------------------------
  // Compute the exact difference at extended precision.
  //
  // Both operands have the same prec p = 128:
  //   value(large) = large.mant * 2^(large.exp - p)
  //   value(small) = small.mant * 2^(small.exp - p)
  //
  // Exact difference = 2^(small.exp - p) * (large.mant * 2^d - small.mant)
  // where d = large.exp - small.exp >= 0.
  //
  // D = large.mant * 2^d - small.mant   (positive bigint)
  // D_exact = D * 2^(small.exp - p)
  //
  // bitLen(D) gives the number of significant bits; the result exponent is:
  //   bxIn = bitLen(D) + (small.exp - p)
  //
  // Ref: mpfr/src/sub1sp.c L861-L923 (d < GMP_NUMB_BITS branch) and
  //   L925-L1002 (d >= GMP_NUMB_BITS branches).
  // -------------------------------------------------------------------------

  const d = large.exp - small.exp;
  const D = (large.mant << d) - small.mant;

  if (D === 0n) {
    // Defensive: cmpMag should have returned 0 — guard anyway.
    const zero = rnd === 'RNDD' ? negZero(p) : posZero(p);
    return { value: zero, ternary: 0 };
  }

  const bl = bitLen(D);
  const bxIn = bl + (small.exp - p);

  // -------------------------------------------------------------------------
  // Round D from bl bits down to p bits.
  // Ref: mpfr/src/sub1sp.c L1005-L1053 — rounding section.
  // -------------------------------------------------------------------------

  let mant: bigint;
  let exp: bigint;
  let ternary: Ternary;

  if (bl <= p) {
    // Lossless — D fits in p bits exactly; pad to MSB-alignment at prec.
    mant = D << (p - bl);
    exp = bxIn;
    ternary = 0;
  } else {
    const rounded = roundMantissa(D, bl, bxIn, p, sign, rnd);
    mant = rounded.mant;
    exp = rounded.exp;
    ternary = rounded.ternary;
  }

  // -------------------------------------------------------------------------
  // Underflow check: bx < emin.
  // Ref: mpfr/src/sub1sp.c L1007-L1021 — the underflow block.
  //
  // The C code checks bx < emin before rounding (with unbounded exponent
  // range). For same-precision subtraction, if bx < emin the subtraction
  // is exact, so bxIn is the post-round exponent too. We use bxIn as the
  // pre-round reference for the RNDN narrowing decision, mirroring C's
  // check at L1016-L1019:
  //   if RNDN and (bx < emin-1 OR (ap[1]==HIGHBIT && ap[0]==0)) → RNDZ.
  // In bigint terms: D exactly equals 1 (i.e. only the MSB of p is set)
  // when D << (p - bl) == 2^(p-1), which is D == 1 (bl == 1).
  // -------------------------------------------------------------------------

  if (exp < EMIN_DEFAULT) {
    let effRnd: RoundingMode = rnd;
    const highbit = 1n << (p - 1n);
    // RNDN narrowing: if |a| <= 2^(emin-2)
    //   ↔ bxIn < emin-1, OR (bxIn == emin-1 AND D is a single bit, i.e.
    //     the unnormalized difference was exactly 2^(p-1), which means
    //     D == 1 when bl==1 / mant == highbit when bl <= p)
    const dIsMinimal = (bl <= p) ? (mant === highbit) : false;
    if (rnd === 'RNDN' && (bxIn < EMIN_DEFAULT - 1n || (bxIn === EMIN_DEFAULT - 1n && dIsMinimal))) {
      effRnd = 'RNDZ';
    }
    return mpfr_underflow(p, effRnd, sign);
  }

  // -------------------------------------------------------------------------
  // Overflow check (defensive — |a| <= |b| so overflow should not occur).
  // Ref: mpfr/src/sub1sp.c — note at L1047-L1048:
  //   "bx+1 cannot exceed __gmpfr_emax, since |a| <= |b|".
  // -------------------------------------------------------------------------

  if (exp > EMAX_DEFAULT) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sub1sp2n: unexpected overflow (|a| <= |b| should preclude this)`,
    );
  }

  const value: MPFR = {
    kind: 'normal',
    sign,
    prec: p,
    exp,
    mant,
  };
  return { value, ternary };
}
