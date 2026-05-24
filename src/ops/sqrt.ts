/**
 * ops/sqrt.ts — pure-TS port of MPFR's `mpfr_sqrt`.
 *
 * Square root of an {@link MPFR} value at the caller-supplied target
 * precision, rounded per the rounding mode, returning the canonical
 * `{value, ternary}` shape from src/core.ts (Law 4).
 *
 * C signature
 * -----------
 *
 *   int mpfr_sqrt(mpfr_t rop, mpfr_srcptr x, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - returns the ternary as the function result.
 *
 *   Ref: mpfr/src/sqrt.c L505-L600+ (dispatcher and general algorithm).
 *
 * TS signature
 * ------------
 *
 *   mpfr_sqrt(x: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 *   1. Dispatch:
 *      - NaN → NAN_VALUE, ternary 0.
 *      - ±0 → ±0 with same sign (sqrt(-0) = -0 by IEEE 754 convention,
 *        mirrored byte-for-byte in mpfr/src/sqrt.c L532
 *        `MPFR_SET_SAME_SIGN(r, u)`).
 *      - +Inf → +Inf, ternary 0.
 *      - -Inf → NAN_VALUE.
 *      - Any normal with sign === -1 → NAN_VALUE (sqrt of negative real
 *        is undefined; mpfr/src/sqrt.c L550-L554).
 *      - normal with sign === +1 → real algorithm below.
 *
 *   2. Normal positive case.
 *
 *      The value is x.mant * 2^(x.exp - x.prec), and sqrt(x) is
 *
 *          sqrt(x.mant) * 2^((x.exp - x.prec) / 2)
 *
 *      For the exponent to be an integer in MPFR's frame we need
 *      (x.exp - x.prec) even — equivalently, we may multiply x.mant
 *      by 2 to flip the parity of the exponent. Two cases:
 *
 *        Case A: (x.exp - x.prec) is even.
 *          n = x.mant << (2*(prec+1) - x.prec)  if  2*(prec+1) >= x.prec
 *          n = x.mant >> (x.prec - 2*(prec+1))  otherwise (rare: prec
 *                                                 very small relative
 *                                                 to x.prec)
 *
 *        Case B: (x.exp - x.prec) is odd.
 *          Multiply x.mant by 2 first (i.e. shift left one more) to
 *          flip parity. This adjusts the result-exponent computation
 *          by +1 (we picked up an extra factor of sqrt(2) which is
 *          absorbed into the integer-shift framing).
 *
 *      Common formulation: choose a shift `s` such that the integer
 *      square root of `n = x.mant << s` (or `x.mant >> -s` for s < 0)
 *      produces a root with exactly `prec + 1` significant bits. Then
 *      we have `prec` bits of result plus one round bit; the sticky
 *      bit comes from the isqrt remainder (`root^2 + rem == n`, so
 *      `rem !== 0n` iff the input is not a perfect square at the
 *      current scale).
 *
 *      Implementation strategy (matches the C reference's structure
 *      while using bigint isqrt for the actual root):
 *
 *      a. Let E = x.exp - x.prec (the "low anchor" exponent: the
 *         exponent at which x.mant's LSB sits in the value).
 *      b. We want sqrt(x) = sqrt(x.mant * 2^E) = sqrt(x.mant) *
 *         2^(E/2). For E to be an even integer we may multiply x.mant
 *         by 2 (incrementing E by -1 and adjusting elsewhere). The
 *         result-exponent floor((E + (parity_adjust ? 1 : 0)) / 2)
 *         is the lower bound; the MPFR resultExp is then computed
 *         from the bit-length of the root.
 *      c. To get a root with prec+1 bits, shift x.mant up by enough
 *         that n's bit-length is exactly 2*(prec+1) — call it 2*K
 *         where K = prec+1. Then isqrt(n) has bit-length K (since
 *         isqrt of a 2K-bit number has K bits).
 *
 *         Specifically: if x.prec has bit-length p (MSB at bit p-1
 *         of x.mant), to make n have bit-length 2*K we shift by
 *               shift = 2*K - p
 *         provided shift is non-negative AND (E + shift) is even.
 *         Parity: E = x.exp - x.prec; (E + shift) = x.exp - p +
 *         shift = x.exp - p + 2K - p = x.exp + 2K - 2p, which is
 *         even iff x.exp is even.
 *
 *         If x.exp is odd, increment shift by 1 (n -> n*2) to fix
 *         parity. This is a faithful mirror of mpfr/src/sqrt.c L589
 *         `odd_exp = (unsigned int) MPFR_GET_EXP (u) & 1`.
 *
 *         If shift would be negative (x.prec > 2K), we right-shift
 *         and capture the dropped bits as sticky. This can happen
 *         when prec is very small relative to x.prec.
 *
 *      d. Compute (root, rem) = isqrt(n).
 *      e. Determine the bit-length L of `root`. By construction it's
 *         in {K, K-1, K+1} depending on whether x.exp is odd and the
 *         exact MSB position of x.mant. In practice L is exactly K
 *         when n's bit-length is 2K, and K+1 if the parity adjust
 *         shifted us one bit further. The result exp is:
 *
 *           resultExp_low = (E + shift) / 2  (integer; from the
 *                              parity-fixed even E + shift)
 *
 *         The MPFR exponent of the rounded sqrt is resultExp_low + L.
 *
 *      f. Round root (a K-bit value) down to prec bits via
 *         roundMantissa, injecting sticky = (rem !== 0n). Use the
 *         same (root<<1) | sticky padding trick as div.ts.
 *
 *      For implementation simplicity we always shift up (never down):
 *      pick shift = 2*K - x.prec + parityAdjust where parityAdjust
 *      ∈ {0, 1} ensures (E + shift) is even. If 2*K + parityAdjust
 *      < x.prec we instead shift down by (x.prec - 2*K - parityAdjust)
 *      and incorporate dropped bits into sticky. (Path b in the
 *      algorithm comment above.)
 *
 *      bigint isqrt: Newton's method seeded by an initial estimate
 *      proportional to 2^(bitLength(n)/2). Converges in O(log n)
 *      iterations.
 *
 *   3. Sign of normal result: always +1 (sqrt of any positive normal
 *      is positive). The validate() invariants reject sign=-1 on
 *      kind='normal' through the value model's MSB-aligned mantissa
 *      check.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sqrt.c L505-L600+ — top-level mpfr_sqrt: singular
 *     dispatch (L522-L554), parity handling (L589), main loop (L590+).
 *   - mpfr/src/sqrt.c L550-L554 — negative-finite case (returns NaN).
 *   - mpfr/src/sqrt.c L532 — sqrt(-0) preserves sign.
 *   - mpfr/src/sqrt1.c, sqrt2.c — prec-class fast paths.
 *   - src/core.ts §"validate" — output invariants.
 *   - src/ops/div.ts — reference for the (prec+2)-bit padding trick
 *     that injects a sticky bit cleanly into roundMantissa's drop
 *     frame.
 *   - src/internal/mpfr/round_raw.ts — the substrate primitive used
 *     to round the (prec+1)-bit root to prec bits.
 *   - CLAUDE.md "Hallucination-risk callouts" — sqrt(-0) = -0;
 *     sqrt(-finite) = NaN; ternary direction is sign of (rounded -
 *     exact); rounding-mode count is FIVE.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negZero,
  posInf,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Validate the public-boundary arguments. Same shape as add/mul/div.
 */
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
 * Bit-length of a non-negative bigint. Same form as div.ts / mul.ts.
 */
function bitLength(v: bigint): bigint {
  if (v <= 0n) return 0n;
  return BigInt(v.toString(2).length);
}

/**
 * Integer square root with remainder. Returns `{root, remainder}` such
 * that `root * root + remainder == n` and `root * root <= n <
 * (root + 1) * (root + 1)`.
 *
 * Algorithm: Newton's method (Heron's algorithm) seeded with a
 * power-of-2 estimate based on `bitLength(n)`. Convergence is
 * quadratic; for n of bit-length B the iteration count is O(log B).
 *
 * The seed `x0 = 2^((bitLength(n) + 1) >> 1)` is an upper bound on the
 * true root (since `2^B > n` implies `2^(B/2) > sqrt(n)`), so the
 * Newton iteration is monotonically decreasing — we detect convergence
 * by `y >= x` (the new estimate has stopped decreasing).
 *
 * Throws `MPFRError('EPREC', ...)` on negative input (defensive; the
 * caller should never reach this with a negative number since the
 * sign=-1 branch is dispatched to NaN above).
 *
 * Ref: Knuth TAOCP vol. 2 §4.7 "Manipulation of Power Series"
 *   exercise 15 — Newton's method for integer sqrt with monotone
 *   convergence proof.
 */
function isqrt(n: bigint): { root: bigint; remainder: bigint } {
  if (n < 0n) {
    throw new MPFRError('EPREC', `isqrt: negative input ${n}`);
  }
  if (n < 2n) {
    // 0 -> (0, 0); 1 -> (1, 0).
    return { root: n, remainder: 0n };
  }
  // Initial seed: 2^((bitLength(n) + 1) / 2), which is >= sqrt(n).
  const B = bitLength(n);
  let x = 1n << ((B + 1n) >> 1n);
  // Newton iteration: x_{i+1} = (x_i + n / x_i) / 2.
  while (true) {
    const y = (x + n / x) >> 1n;
    if (y >= x) {
      // x is the integer floor of sqrt(n).
      return { root: x, remainder: n - x * x };
    }
    x = y;
  }
}

/**
 * Square root core for a normal positive MPFR. Returns the
 * {value, ternary} pair at the target precision.
 *
 * Pre-conditions:
 *   - `x` is kind:'normal' with sign === 1.
 *   - `prec >= 1`, `rnd` is a valid RoundingMode.
 */
function sqrtNormalPositive(
  x: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // We want isqrt(n) to have bit-length prec+1 (so we get prec target
  // bits + 1 round bit, with sticky from the isqrt remainder). For
  // that, n's bit-length must be in {2(prec+1), 2(prec+1) - 1}: a
  // 2K-bit n has isqrt of bit-length K, and a (2K-1)-bit n has isqrt
  // of bit-length K-1 (the largest (K-1)-bit value's square is
  // (2^(K-1) - 1)^2 < 2^(2K-1)). We need the larger case (n has
  // bit-length exactly 2K) — so we shift x.mant up by exactly the
  // amount that puts its MSB at bit (2K - 1) of n. With x.mant's MSB
  // at bit (x.prec - 1) of x.mant, the shift is
  //     shift = (2K - 1) - (x.prec - 1) = 2K - x.prec
  // where K = prec + 1.
  //
  // Parity: the result exponent must be integer. The exact value
  // satisfies sqrt(x) = sqrt(x.mant * 2^(x.exp - x.prec)) =
  // sqrt(x.mant * 2^(- shift) * 2^(x.exp - x.prec + shift)) =
  // sqrt(n) * 2^((x.exp - x.prec + shift) / 2 - shift / 2) when n =
  // x.mant << shift. Simpler: define
  //     n = x.mant << shift
  //     a = x.exp - x.prec + shift  (the "anchor" exponent, in bits)
  //     sqrt(x) = sqrt(n) * 2^(a / 2 - shift / 2)
  //             = sqrt(n) * 2^((a - shift) / 2)
  //             = sqrt(n) * 2^((x.exp - x.prec) / 2)
  // ... no, that drops the n-vs-x.mant scaling. Let's redo it.
  //
  // |x| = x.mant * 2^(x.exp - x.prec).
  // Let n = x.mant << shift. Then |x| = n * 2^(x.exp - x.prec - shift).
  // sqrt(|x|) = sqrt(n) * 2^((x.exp - x.prec - shift) / 2).
  // For the exponent to be integer, (x.exp - x.prec - shift) must be
  // even. With shift = 2K - x.prec, we get (x.exp - x.prec - shift) =
  // x.exp - x.prec - 2K + x.prec = x.exp - 2K. The parity of this is
  // the parity of x.exp. If x.exp is odd, we must adjust — typically
  // by adding 1 to shift (turning n into 2n, which doubles |x|'s
  // "anchor" position by 1 and flips parity), and accepting that the
  // root's bit-length becomes K (still) or K+1.
  //
  // Implementation: form n = x.mant << s where s = shift +
  // parityAdjust, with parityAdjust = (x.exp odd ? 1 : 0). Then
  // (x.exp - x.prec - s) = x.exp - 2K - parityAdjust, which is even
  // by construction. sqrt(|x|) = sqrt(n) * 2^(((x.exp - 2K -
  // parityAdjust) / 2)). The MPFR exp of the rounded result is
  //     resultExp = exp_low + L
  // where exp_low = (x.exp - 2K - parityAdjust) / 2 and L is the
  // bit-length of the root after isqrt.
  //
  // For x.exp odd, parityAdjust = 1, n becomes 2 * (x.mant << shift),
  // which has bit-length 2K + 1. Its isqrt has bit-length K + 1
  // (since (2^K)^2 = 2^(2K) and (2^K + small)^2 ~ 2^(2K) but our n is
  // in [2^(2K), 2^(2K+1)), so the root is in [2^K, 2^((K+1) -
  // epsilon)) — actually bit-length K. Wait: (2^K)^2 = 2^(2K) which
  // is <= n < 2^(2K+1), and ((2^K + a))^2 ~ 2^(2K) + ..., so root
  // is between 2^K and 2^K * sqrt(2) ~ 2^K * 1.414 < 2^(K+1). So
  // root's bit-length is K + 1.
  //
  // Hmm — bit-length-K means MSB at bit (K-1); bit-length-(K+1) means
  // MSB at bit K. For x.exp even, n has bit-length 2K, root has
  // bit-length K. For x.exp odd, n has bit-length 2K+1, root has
  // bit-length K+1.
  //
  // We want srcPrec = bit-length of root = (K or K+1). The (prec+2)-
  // bit padding trick (similar to div.ts) handles both: pad with the
  // sticky bit and call roundMantissa with srcPrec = (rootBitLength +
  // 1), outPrec = prec.
  const K = prec + 1n;
  const shift = 2n * K - x.prec;
  const parityAdjust = (x.exp & 1n) === 0n ? 0n : 1n;
  const s = shift + parityAdjust;

  let n: bigint;
  let extraSticky = false;
  if (s >= 0n) {
    n = x.mant << s;
  } else {
    // Right-shift: rare case where x.prec > 2K + parityAdjust (the
    // input is much more precise than we need). Capture dropped bits
    // as sticky.
    const rshift = -s;
    n = x.mant >> rshift;
    const droppedMask = (1n << rshift) - 1n;
    if ((x.mant & droppedMask) !== 0n) {
      extraSticky = true;
    }
  }

  const { root, remainder } = isqrt(n);

  // sticky = (remainder !== 0n) || extraSticky.
  // The exact value satisfies n = root^2 + remainder; the exact sqrt
  // is root iff remainder == 0 (and no bits were dropped from x.mant
  // in the right-shift case).
  const sticky = remainder !== 0n || extraSticky;

  // Result exp computation: exp_low = (x.exp - 2K - parityAdjust) / 2,
  // and the MPFR exp of the rounded result is exp_low + L (root
  // bit-length). For x.exp even, exp_low = (x.exp - 2K) / 2, L = K,
  // so resultExp = (x.exp - 2K)/2 + K = x.exp/2. For x.exp odd,
  // exp_low = (x.exp - 2K - 1)/2, L = K+1, so resultExp = (x.exp - 2K
  // - 1)/2 + (K + 1) = (x.exp + 1)/2. Combined:
  //     resultExp = ceil((x.exp + 1) / 2)  for x.exp odd
  //     resultExp = x.exp / 2              for x.exp even
  // ... equivalently:
  //     resultExp = (x.exp + parityAdjust) / 2 + (parityAdjust ? 0 : 0)
  // Wait, let me just compute it directly from `(x.exp + 1) / 2` for
  // x.exp odd: (x.exp + 1) is even, so this gives an integer. For
  // x.exp even, x.exp / 2 gives an integer. The unified form is
  //     resultExp = (x.exp + parityAdjust + 1) / 2  ... no.
  // For x.exp = 2k: resultExp = k = x.exp / 2.
  // For x.exp = 2k+1: resultExp = k+1 = (x.exp + 1) / 2.
  // Unified: resultExp = (x.exp + 1) >> 1 if x.exp odd, else x.exp >> 1.
  // Equivalently: resultExp = (x.exp + parityAdjust) >> 1 (since
  // parityAdjust = x.exp & 1). Let's verify: x.exp=4, parityAdjust=0,
  // resultExp = 4 >> 1 = 2. ✓. x.exp=5, parityAdjust=1, resultExp =
  // 6 >> 1 = 3. ✓. Good.
  //
  // (Bigint division rounds toward zero for positive, but x.exp can
  // be negative for small magnitudes. The MPFR exp of sqrt is
  // ceil(x.exp / 2), which for negative x.exp uses arithmetic-shift
  // semantics, which bigint `>>` does NOT provide for negative
  // bigints in the same way as 2's-complement integer. Safer:
  // resultExp_low = (x.exp - 2K - parityAdjust) / 2; resultExp =
  // resultExp_low + L. Compute it that way directly.)
  const L = bitLength(root);
  const expLow = (x.exp - 2n * K - parityAdjust) / 2n;
  const resultExp = expLow + L;

  // Round root (an L-bit value) to prec bits via the (L+1)-bit
  // padding trick — same as div.ts.
  const stickyBit = sticky ? 1n : 0n;
  const padded = (root << 1n) | stickyBit;
  const srcPrec = L + 1n;

  // Edge case: when L <= prec the lossless path applies — but the
  // padded value has bit L + 1, so srcPrec = L + 1 vs outPrec = prec
  // means we're dropping (L + 1 - prec) bits. When L + 1 <= prec
  // we're actually expanding, which roundMantissa doesn't handle
  // (its contract requires srcPrec > outPrec). This happens when L
  // <= prec - 1, i.e. the root naturally fits in fewer bits than we
  // asked for — only possible when prec is large relative to x.prec
  // and parityAdjust is 0, since otherwise L = K = prec + 1.
  //
  // Defensive: L should always be in {K, K+1} = {prec+1, prec+2}.
  // The L+1 srcPrec is therefore in {prec+2, prec+3}, both > prec.
  // The exact value of sticky doesn't matter when remainder == 0 and
  // extraSticky == false; in that case the value IS exact and the
  // padding is `padded = root << 1 | 0`, srcPrec = L + 1, and
  // roundMantissa returns the value unchanged with ternary 0 — even
  // when dropping bits, the dropped bits are all zero by construction.
  if (L < prec + 1n) {
    // Should be unreachable given the parity-adjusted shift. Surface
    // precisely.
    throw new MPFRError(
      'EPREC',
      `sqrtNormalPositive: unexpected root bit-length L=${L} (expected ${prec + 1n} or ${prec + 2n})`,
    );
  }

  const sign: Sign = 1;
  const { mant, exp, ternary } = roundMantissa(
    padded,
    srcPrec,
    resultExp,
    prec,
    sign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Square root at the target precision, returning the rounded result
 * and the ternary flag (sign of `(rounded - exact)`).
 *
 * @mpfrName mpfr_sqrt
 *
 * @param x     operand. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. The value passes `validate()` without
 *              post-processing. Ternary is `0` for exact (including all
 *              specials), `+1` if rounded > exact, `-1` if rounded < exact.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *                    NaN / Inf / zero / negative input is NOT an error.
 *
 * @example
 *   sqrt(setD(4.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 2.0 at prec 53, ternary: 0}
 *   sqrt(negZero(53n), 53n, 'RNDN');
 *     // → {value: -0 at prec 53, ternary: 0}  — IEEE 754 sign-preserving
 *   sqrt(setD(-1.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}  — sqrt of negative
 *   sqrt(setD(2.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: ~1.4142..., ternary: -1}  — rounded down from exact sqrt(2)
 */
export function mpfr_sqrt(
  x: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // --- Specials ---------------------------------------------------------
  // (1) NaN propagation.
  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) ±0 → ±0 with same sign. Mirrors mpfr/src/sqrt.c L529-L535:
  // MPFR_SET_SAME_SIGN(r, u); MPFR_SET_ZERO(r); ternary 0.
  if (x.kind === 'zero') {
    return {
      value: x.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (3) ±Inf. +Inf → +Inf, -Inf → NaN. Mirrors mpfr/src/sqrt.c
  // L538-L548.
  if (x.kind === 'inf') {
    if (x.sign === -1) {
      return { value: NAN_VALUE, ternary: 0 };
    }
    return { value: posInf(prec), ternary: 0 };
  }

  // (4) Negative normal → NaN. Mirrors mpfr/src/sqrt.c L550-L554.
  if (x.kind === 'normal' && x.sign === -1) {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (5) Positive normal — the algebraic core.
  if (x.kind !== 'normal') {
    // Defensive: unreachable. All other kinds dispatched above.
    throw new MPFRError(
      'EPREC',
      `mpfr_sqrt: unexpected kind=${x.kind} at normal-positive branch`,
    );
  }
  return sqrtNormalPositive(x, prec, rnd);
}
