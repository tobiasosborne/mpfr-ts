/**
 * ops/add1sp3.ts — pure-TS port of MPFR's `mpfr_add1sp3`.
 *
 * The same-precision, same-sign addition fast path for
 * `2 * GMP_NUMB_BITS < p < 3 * GMP_NUMB_BITS` (= 129 <= p <= 191 on x86_64),
 * i.e. three-limb mantissas with non-zero sh = 192 - p in [1, 63].
 * The dispatcher `mpfr_add1sp` (add1sp.c L1493-L1494) routes here when
 * `2 * GMP_NUMB_BITS < p < 3 * GMP_NUMB_BITS`.
 *
 * C signature
 * -----------
 *
 *   static int mpfr_add1sp3(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                            mpfr_rnd_t rnd_mode, mpfr_prec_t p)
 *
 * TS signature
 * ------------
 *
 *   mpfr_add1sp3(b, c, rnd) -> Result
 *
 * Pre-conditions
 * --------------
 *
 *   - `b.kind === 'normal'` and `c.kind === 'normal'`
 *   - `b.sign === c.sign` (same-sign; caller ensures this)
 *   - `b.prec === c.prec` and `129n <= b.prec <= 191n`
 *
 * Strategy
 * --------
 *
 * The TS port uses **bigint extended-precision arithmetic** instead of
 * decomposing into three 64-bit limbs. The exact mathematical sum at
 * unbounded precision is:
 *
 *     S * 2^(small.exp - p) = b.mant * 2^(b.exp - p) + c.mant * 2^(c.exp - p)
 *
 * Setting `d = large.exp - small.exp >= 0`, we have:
 *
 *     S = (large.mant << d) + small.mant      (exact bigint sum)
 *
 * The bit length of S is either `p + d` (no carry past the top bit) or
 * `p + d + 1` (carry-out, i.e. addition produced a value in
 * [2^(p+d), 2^(p+d+1)) ). The result exponent is:
 *
 *     bx_result = large.exp + (bitLen(S) - (p + d))
 *
 * — which is either `large.exp` or `large.exp + 1`.
 *
 * Then S is rounded to `prec` bits via `roundMantissa`, which handles
 * the round/sticky/half/even logic uniformly across modes. The carry
 * case (LSB increment overflows past 2^prec) is handled inside
 * roundMantissa by bumping the result exp by 1.
 *
 * Overflow on exp > emax delegated to mpfr_overflow.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add1sp.c L613-L770 — verbatim C reference body for add1sp3.
 *   - mpfr/src/add1sp.c L1493-L1494 — dispatcher routing.
 *   - src/ops/add1sp2.ts — sister op for 65..127 (same bigint pattern).
 *   - src/ops/sub1sp3.ts — companion op (sub direction) at same prec range.
 *   - src/internal/mpfr/round_raw.ts — substrate rounding primitive.
 *   - src/ops/overflow.ts — delegate on bx > emax.
 *   - src/core.ts — locked schema.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../src/core.ts';
import { MPFRError } from '../../src/core.ts';
import { roundMantissa } from '../../src/internal/mpfr/round_raw.ts';
import { mpfr_overflow } from '../../src/ops/overflow.ts';

// ---------------------------------------------------------------------------
// Constants
// Ref: mpfr/src/mpfr.h L231 — MPFR_EMAX_DEFAULT = (1 << 30) - 1.
// ---------------------------------------------------------------------------

const EMAX_DEFAULT = (1n << 30n) - 1n;

/** Bit length of a positive bigint. Returns 0 for 0. */
function bitLen(x: bigint): bigint {
  if (x <= 0n) return 0n;
  return BigInt(x.toString(2).length);
}

// ---------------------------------------------------------------------------
// Public port
// ---------------------------------------------------------------------------

/**
 * Same-precision same-sign addition fast path for `129 <= p <= 191`.
 *
 * Mirrors `mpfr_add1sp3` from `mpfr/src/add1sp.c L613-L770` at the I/O
 * contract level. The TS implementation uses bigint extended-precision
 * arithmetic rather than the C's 3-limb representation; the rounded
 * result is identical because the round/sticky/half/even logic operates
 * on the same exact-sum bit pattern.
 *
 * @mpfrName mpfr_add1sp3
 *
 * @param b   First addend; `kind='normal'`, `129 <= prec <= 191`.
 * @param c   Second addend; same kind, sign, and prec.
 * @param rnd Rounding mode.
 *
 * @returns `{ value, ternary }`.
 *
 * @throws {MPFRError} `EPREC` on invalid kind/prec; `EROUND` on bad rnd.
 *
 * Ref: mpfr/src/add1sp.c L613-L770 — C reference body.
 * Ref: mpfr/src/add1sp.c L1493-L1494 — dispatcher routing.
 */
export function mpfr_add1sp3(
  b: MPFR,
  c: MPFR,
  rnd: RoundingMode,
): Result {
  // -------------------------------------------------------------------------
  // Validate preconditions
  // Ref: mpfr/src/add1sp.c L630 — MPFR_ASSERTD(2*GMP_NUMB_BITS < p && p < 3*GMP_NUMB_BITS).
  // -------------------------------------------------------------------------
  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp3: b.kind must be 'normal', got '${b.kind}'`);
  }
  if (c.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_add1sp3: c.kind must be 'normal', got '${c.kind}'`);
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', `mpfr_add1sp3: b.prec (${b.prec}) !== c.prec (${c.prec})`);
  }
  const p = b.prec;
  if (p <= 128n || p >= 192n) {
    throw new MPFRError('EPREC', `mpfr_add1sp3: prec must be in (128, 192), got ${p}`);
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', `mpfr_add1sp3: b.sign !== c.sign`);
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_add1sp3: unknown rounding mode '${String(rnd)}'`);
  }

  // -------------------------------------------------------------------------
  // Compute the exact extended-precision sum.
  // Ref: mpfr/src/add1sp.c L632-L733 — the C version's case analysis
  //   (bx==cx / swap-to-bx>cx / d<64 / 64<=d<128 / 128<=d<192 / d>=192)
  //   all reduce to "compute S = (large.mant << d) + small.mant" at the
  //   bigint level.
  // -------------------------------------------------------------------------
  const sign = b.sign;
  let large: MPFR, small: MPFR;
  if (b.exp >= c.exp) {
    large = b; small = c;
  } else {
    large = c; small = b;
  }
  const d = large.exp - small.exp;  // d >= 0

  // S = (large.mant << d) + small.mant.
  // Since large.mant has bit (p-1) set, S has bit (p-1+d) set; carry into
  // bit (p+d) may occur if both operands have their MSB set.
  const S = (large.mant << d) + small.mant;
  const bl = bitLen(S);  // bl in {p + d, p + d + 1}

  // Result exponent: large.exp + (bl - (p + d)) = large.exp + carry.
  // No carry: bl = p + d → bx_result = large.exp.
  // Carry: bl = p + d + 1 → bx_result = large.exp + 1.
  const bxResult = large.exp + (bl - (p + d));

  // -------------------------------------------------------------------------
  // Round S (bl bits) to p bits.
  // -------------------------------------------------------------------------
  let mant: bigint;
  let exp: bigint;
  let ternary: Ternary;
  if (bl <= p) {
    // Lossless — S already fits in p bits (only possible when d=0 and no carry
    // happened; but in same-sign add with both MSBs set and equal exp, carry
    // ALWAYS happens, so this branch is essentially unreachable for valid inputs).
    // Defensive: pad to p bits.
    mant = S << (p - bl);
    exp = bxResult;
    ternary = 0;
  } else {
    const rounded = roundMantissa(S, bl, bxResult, p, sign, rnd);
    mant = rounded.mant;
    exp = rounded.exp;
    ternary = rounded.ternary;
  }

  // -------------------------------------------------------------------------
  // Overflow check (post-rounding).
  // Ref: mpfr/src/add1sp.c L735-L737 — bx > emax check before setting result.
  // -------------------------------------------------------------------------
  if (exp > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
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
