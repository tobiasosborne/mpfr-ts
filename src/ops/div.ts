/**
 * ops/div.ts — pure-TS port of MPFR's `mpfr_div`.
 *
 * Divide two {@link MPFR} values at the caller-supplied target precision,
 * rounded per the rounding mode, returning the canonical
 * `{value, ternary}` shape from src/core.ts (Law 4).
 *
 * C signature
 * -----------
 *
 *   int mpfr_div(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - returns the ternary as the function result.
 *
 *   Ref: mpfr/src/div.c L740-L848 (dispatcher) and L860+ (general
 *   algorithm).
 *
 * TS signature
 * ------------
 *
 *   mpfr_div(a: MPFR, b: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `prec` as an explicit positional argument (no `rop`);
 *   - returns the immutable {@link Result} from src/core.ts.
 *
 * Algorithm
 * ---------
 *
 * Top-level dispatch on (a.kind, b.kind), then normal/normal core:
 *
 *   1. NaN propagation. NaN / anything or anything / NaN → NAN_VALUE.
 *
 *   2. ±Inf handling.
 *      - Inf / Inf  → NaN (indeterminate, mpfr/src/div.c L794-L798).
 *      - Inf / finite (zero or normal) → sign-product Inf, ternary 0.
 *      - finite / Inf → sign-product zero, ternary 0.
 *
 *   3. ±0 / ±0 → NaN (indeterminate, mpfr/src/div.c L812-L815).
 *
 *   4. ±0 / +-finite-nonzero → sign-product zero, ternary 0.
 *
 *   5. +-finite-nonzero / ±0 → sign-product Inf (the "divbyzero" branch
 *      of mpfr/src/div.c L817-L823 — MPFR sets the divbyzero flag here
 *      but the *value* returned is sign-product Inf; the TS port honours
 *      the value contract without surfacing a flag).
 *
 *   6. normal / normal — the algebraic core.
 *
 *      Let a, b be MPFR normals with values
 *           a = a.sign * a.mant * 2^(a.exp - a.prec)
 *           b = b.sign * b.mant * 2^(b.exp - b.prec)
 *
 *      The exact quotient is therefore
 *           a/b = (a.sign * b.sign) * (a.mant / b.mant)
 *                 * 2^((a.exp - a.prec) - (b.exp - b.prec))
 *
 *      Strategy:
 *        a. resultSign = a.sign * b.sign  (the product-of-signs rule).
 *        b. Compute the bigint quotient and remainder of
 *               num = a.mant << shift
 *               quot, rem = (num / b.mant, num % b.mant)
 *           with `shift` chosen so the quotient `quot` has exactly
 *           `prec + 1` significant bits — i.e. one round bit and a
 *           remainder-derived sticky.
 *
 *           Bit-length of (a.mant / b.mant) without any shift is in
 *           {0, 1} (since a.mant, b.mant in [2^(p-1), 2^p) for their
 *           respective prec p, the ratio is in (1/2, 2)). To get
 *           prec+1 bits of quotient we must shift up by enough that
 *           the bit-length of `num / b.mant` becomes prec+1. That
 *           requires
 *               shift = prec + 1 + b.prec - a.prec + tweak,
 *           where `tweak ∈ {0, 1}` depending on whether
 *           a.mant >= b.mant (post-shift the quotient has prec+1 bits)
 *           or a.mant < b.mant (it has prec bits and we need +1 more
 *           shift). We resolve this by always shifting by
 *               shift = prec + 1 + b.prec - a.prec
 *           and then checking the resulting bit-length L = bitLength(quot)
 *           ∈ {prec, prec+1}; if L == prec, we left-shift one more bit
 *           (multiply `num` by 2, which conceptually pulls in another
 *           sticky-or-round bit from below). The equivalent maneuver in
 *           the C reference is detecting `cmp(up, vp) >= 0` and adjusting
 *           the result exponent (mpfr/src/div.c L890-L920).
 *
 *        c. Result exponent: in MPFR's frame, the exact value is
 *               sign * (a.mant / b.mant) * 2^((a.exp - a.prec) - (b.exp - b.prec))
 *           and once we've normalised the quotient to L = prec+1 bits
 *           via the shift above, the MPFR exp of the rounded value is
 *               resultExp = a.exp - b.exp + (L_post - shift + (b.prec - a.prec))
 *           where L_post = prec + 1 after normalisation. Substituting
 *               shift = prec + 1 + b.prec - a.prec
 *           we get
 *               resultExp = a.exp - b.exp + (prec + 1) - (prec + 1 + b.prec - a.prec) + (b.prec - a.prec)
 *                         = a.exp - b.exp.
 *           If we had to apply the `+1 more shift` correction (i.e.
 *           a.mant < b.mant case), the result exponent decrements by 1:
 *               resultExp = a.exp - b.exp - 1.
 *
 *           Geometric sanity: |a/b| lies in (|a|*2/|b|, |a|/|b|*2);
 *           with a.mant >= b.mant the MSB of the quotient is at the
 *           same position as a.mant's relative to b.mant's, giving
 *           resultExp = a.exp - b.exp. When a.mant < b.mant the
 *           quotient's MSB is one bit lower, giving resultExp = a.exp
 *           - b.exp - 1.
 *
 *        d. Round the (prec+1)-bit quotient to prec bits via
 *           roundMantissa. The sticky bit is injected by ORing
 *           (rem !== 0n ? 1n : 0n) into the LSB of `quot` BEFORE
 *           handing it to roundMantissa: this is faithful to MPFR's
 *           `round_bit | sticky` trick (mpfr/src/div.c L1064-L1080),
 *           where the sticky bit collapses any nonzero remainder into
 *           the next-lowest bit position so the rounding decision
 *           inspects "did we drop ANY 1 bit below the round position".
 *
 *           However: this OR-into-LSB trick is only safe when the
 *           round-bit lookup itself happens at the LSB of quot —
 *           which is exactly what roundMantissa does when called with
 *           srcPrec=prec+1, outPrec=prec. The dropped bit is then
 *           `quot & 1`, and our sticky is folded directly into that
 *           position. For RNDN tie-breaking, `quot & 1 == 0` together
 *           with `rem != 0` means "barely above half" (not a tie);
 *           we encode this by setting bit 0 to (oldBit0 | (rem!=0))
 *           and bit 1 to oldBit1. The original bit-1 (the "round-to-
 *           even" parity for RNDN ties) survives. Subtle but correct.
 *
 *           Wait — clearer formulation: the (prec+1)-bit quotient has
 *           bits [prec..0] with bit `prec` being the MSB and bit `0`
 *           the LSB. roundMantissa drops bit 0; for RNDN, it consults
 *           bit 0 (the round bit) and the bits below (sticky). Since
 *           there are no bits below position 0 in our (prec+1)-bit
 *           value, the sticky must come from the division remainder.
 *           Solution: synthesize a (prec+2)-bit value where bit 1 is
 *           the original bit 0 (the round bit), and bit 0 is the
 *           sticky. Then call roundMantissa with srcPrec=prec+2,
 *           outPrec=prec, which drops the lowest 2 bits — its
 *           "dropped" mask is bits[1..0] = (roundBit << 1) | sticky.
 *           roundMantissa's bit-test logic treats this correctly:
 *           dropped > half == (roundBit << 1) | sticky > (1 << 1),
 *           i.e. "round up if roundBit && (sticky || ...)". Exactly
 *           the IEEE 754 round-to-nearest-even rule.
 *
 *           Implementation: form `padded = (quot << 1) | sticky` where
 *           sticky = (rem !== 0n ? 1n : 0n) and feed roundMantissa
 *           with srcPrec=prec+2 (the bit count of `padded`).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/div.c L740-L848 — top-level dispatcher and singular
 *     case dispatch (L783-L831).
 *   - mpfr/src/div.c L860-L1100 — general algorithm: limb-array
 *     shift-and-divide; reference for the round_bit/sticky decomposition.
 *   - mpfr/src/div_1.c, div_2.c — prec-class fast paths; reference for
 *     the umul_ppmm-based one-limb quotient.
 *   - src/core.ts §"validate" — output invariants every returned MPFR
 *     must satisfy.
 *   - src/ops/mul.ts — reference for the arithmetic-class structure
 *     (specials dispatch, packNormal helper, product-of-signs sign rule).
 *   - src/internal/mpfr/round_raw.ts — the substrate primitive used to
 *     drop the round+sticky bits to the target precision.
 *   - CLAUDE.md "Hallucination-risk callouts" — 0/0=NaN, Inf/Inf=NaN,
 *     finite/0=Inf (sign-product), ternary direction is sign of (rounded
 *     - exact), rounding-mode count is FIVE, sign rule is product-of-signs.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Validate the public-boundary arguments. See add.ts / mul.ts for the
 * rationale: only the scalar `prec` / `rnd` arguments are checked here;
 * MPFR-typed inputs are trusted (validated upstream by the grader's
 * decodeMpfr or by library-internal construction).
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
 * The product-of-signs rule. Mirrors MPFR's `MPFR_MULT_SIGN` macro: the
 * sign of a/b is the product of signs (same convention as multiplication).
 */
function multSign(a: Sign, b: Sign): Sign {
  return (a * b) as Sign;
}

/**
 * Number of significant bits in `v`. For `v > 0` this is the position
 * (1-indexed) of the topmost set bit. For `v === 0n` returns `0n`.
 *
 * Used to detect whether the bigint quotient has bit-length `prec+1`
 * (a.mant >= b.mant case) or `prec` (a.mant < b.mant case); the
 * decision feeds the exponent computation and the one-extra-shift
 * normalisation step in `divNormalNormal`.
 *
 * Uses bigint's `toString(2).length` which is the fastest portable
 * bit-length for bigint in V8/Bun. Same form as mul.ts.
 */
function bitLength(v: bigint): bigint {
  if (v <= 0n) return 0n;
  return BigInt(v.toString(2).length);
}

/**
 * Divide two normal MPFR values; return the {value, ternary} pair at
 * the target precision.
 *
 * Strategy: compute the bigint quotient with enough shift to retain
 * `prec + 1` bits (or `prec + 2` in the carry case), fold the division
 * remainder into a sticky bit, then round one bit down via roundMantissa.
 *
 * Algebra
 * -------
 *
 * Let
 *   |a| = a.mant * 2^(a.exp - a.prec)
 *   |b| = b.mant * 2^(b.exp - b.prec)
 *   |a/b| = (a.mant / b.mant) * 2^((a.exp - a.prec) - (b.exp - b.prec))
 *
 * Choose `shift = prec + 1 + b.prec - a.prec` (assume >= 0 for now;
 * the shift < 0 case right-shifts and folds dropped bits into sticky).
 *
 *   num = a.mant << shift       (bit-length = a.prec + shift = prec + 1 + b.prec)
 *   divisor = b.mant            (bit-length = b.prec)
 *   quot = num / divisor        (bit-length L ∈ {prec+1, prec+2})
 *   rem  = num % divisor
 *   quot = (a.mant/b.mant) * 2^shift  (approximately, with floor)
 *
 * Then
 *   |a/b| = quot * 2^(- shift + a.exp - a.prec - b.exp + b.prec)
 *         = quot * 2^(a.exp - b.exp - prec - 1)
 *
 * In MPFR's frame, |a/b| ∈ [2^(resultExp - 1), 2^resultExp), and the
 * bit-length of quot is L (MSB at position L-1), so:
 *   resultExp = (a.exp - b.exp - prec - 1) + L
 *   - For L = prec+1: resultExp = a.exp - b.exp.
 *   - For L = prec+2: resultExp = a.exp - b.exp + 1.
 *
 * Bit-length-L of quot:
 *   bitLength(num) - bitLength(divisor) ∈ {L-1, L} as a standard
 *   property of integer division. With bitLength(num) = prec + 1 +
 *   b.prec and bitLength(divisor) = b.prec, L ∈ {prec+1, prec+2}.
 *
 * In the L = prec+2 case we have one extra bit at the top: we right-
 * shift quot by 1 bit, accumulating the dropped LSB into the sticky.
 * The final (prec+1)-bit value goes through the same (prec+2)-bit
 * padding trick as before.
 *
 * Pre-conditions:
 *   - `a` and `b` are kind:'normal' (so a.mant, b.mant are MSB-aligned).
 *   - `prec >= 1`, `rnd` is a valid RoundingMode.
 */
function divNormalNormal(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Product-of-signs rule applies uniformly.
  const resultSign: Sign = multSign(a.sign, b.sign);

  const shift = prec + 1n + b.prec - a.prec;

  // `shift` can be negative when a.prec is large relative to prec +
  // b.prec; in that case we right-shift the dividend and capture the
  // bits being shifted off as part of the sticky. The cleanest
  // formulation is to always work in terms of (num, divisor) where
  // num is what we divide and divisor is b.mant (possibly left-shifted
  // when shift < 0).
  let num: bigint;
  let divisor: bigint;
  if (shift >= 0n) {
    num = a.mant << shift;
    divisor = b.mant;
  } else {
    num = a.mant;
    divisor = b.mant << -shift;
  }

  let quot = num / divisor;
  let rem = num % divisor;

  // Bit-length of quot is in {prec+1, prec+2}.
  let L = bitLength(quot);
  if (L !== prec + 1n && L !== prec + 2n) {
    // Defensive: a contract violation in the shift accounting above
    // would land here.
    throw new MPFRError(
      'EPREC',
      `divNormalNormal: unexpected quotient bit-length L=${L} (expected ${prec + 1n} or ${prec + 2n})`,
    );
  }

  let resultExp: bigint;
  if (L === prec + 1n) {
    resultExp = a.exp - b.exp;
  } else {
    // L == prec + 2. Right-shift quot by 1, fold the dropped bit into
    // sticky. The shifted-out bit is `quot & 1`; if it's 1, the
    // resulting `quot >> 1` is missing one ulp's worth of value that
    // gets recorded as a sticky-1 bit. Equivalently we may compute
    //     rem_eff = (quot & 1) * divisor + rem
    // which is the residual that would have been left if we had
    // divided by (divisor << 1) directly. Sticky is (rem_eff != 0),
    // i.e. (quot & 1) || (rem != 0).
    const droppedBit = quot & 1n;
    quot >>= 1n;
    // The new remainder (against divisor << 1) is droppedBit*divisor + rem.
    // For sticky purposes only its zero/nonzero status matters:
    if (droppedBit !== 0n) {
      // droppedBit*divisor is nonzero (divisor > 0), so sticky is nonzero.
      // We can collapse `rem` to a sentinel nonzero to signal sticky.
      rem = 1n;
    }
    // else: rem unchanged.
    L = prec + 1n;
    resultExp = a.exp - b.exp + 1n;
  }

  return finishDiv(resultSign, resultExp, quot, prec, rnd, rem !== 0n);
}

/**
 * Round a (prec+1)-bit quotient down to `prec` bits with a sticky bit
 * derived from the division remainder. See divNormalNormal's algorithm
 * commentary for the (prec+2)-bit padding trick that lets us inject the
 * sticky cleanly into roundMantissa's drop frame.
 *
 * Pre-conditions:
 *   - `quot` has bit-length exactly `prec + 1` (MSB-aligned to prec+1).
 *   - `sign` is the sign of the unrounded value.
 *   - `prec >= 1`.
 *
 * @param sign        Result sign (1 or -1).
 * @param srcExp      MPFR exp of the unrounded quotient.
 * @param quot        The (prec+1)-bit quotient, non-negative bigint.
 * @param prec        Target output precision in bits.
 * @param rnd         Rounding mode.
 * @param sticky      True iff the division remainder was nonzero.
 */
function finishDiv(
  sign: Sign,
  srcExp: bigint,
  quot: bigint,
  prec: bigint,
  rnd: RoundingMode,
  sticky: boolean,
): Result {
  // Form `padded = (quot << 1) | (sticky ? 1 : 0)`. This is a (prec+2)-bit
  // value whose top `prec` bits are quot's top `prec` bits, whose bit
  // 1 is quot's bit 0 (the round bit), and whose bit 0 is the sticky.
  // Passing it to roundMantissa with srcPrec=prec+2, outPrec=prec drops
  // the lowest 2 bits. roundMantissa's "dropped" mask becomes
  //     dropped = (roundBit << 1) | sticky
  //     half    = 1 << (k - 1) = 1 << 1 = 2
  // so for RNDN: dropped > half (i.e. (roundBit << 1) | sticky > 2)
  // iff roundBit == 1 AND sticky == 1; dropped == half iff roundBit
  // == 1 AND sticky == 0 (a clean tie); dropped < half iff roundBit
  // == 0. This is exactly the IEEE 754 round-to-nearest-even rule.
  //
  // For RNDZ / RNDA / RNDU / RNDD, the round-or-truncate decision only
  // looks at "dropped != 0" (round-or-truncate based on rnd and sign),
  // which still works correctly because the sticky bit ensures
  // (dropped == 0) iff (roundBit == 0 && sticky == false).
  const stickyBit = sticky ? 1n : 0n;
  const padded = (quot << 1n) | stickyBit;
  const srcPrec = prec + 2n;
  const { mant, exp, ternary } = roundMantissa(
    padded,
    srcPrec,
    srcExp,
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
 * Divide two MPFR values at the target precision, returning the rounded
 * result and the ternary flag (sign of `(rounded - exact)`).
 *
 * @mpfrName mpfr_div
 *
 * @param a     dividend. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param b     divisor. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. The value passes `validate()` without
 *              post-processing. Ternary is `0` for exact (including all
 *              specials), `+1` if rounded > exact, `-1` if rounded < exact.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *                    NaN / Inf / zero input is NOT an error.
 *
 * @example
 *   div(setD(6.0, 53n, 'RNDN').value, setD(3.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 2.0 at prec 53, ternary: 0}
 *   div(posZero(53n), posZero(53n), 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}  — 0/0 is indeterminate
 *   div(setD(1.0, 53n, 'RNDN').value, posZero(53n), 53n, 'RNDN');
 *     // → {value: +Inf at prec 53, ternary: 0}  — divbyzero
 *   div(setD(1.0, 53n, 'RNDN').value, setD(3.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: ~0.3333..., ternary: -1}  — rounded down from exact 1/3
 */
export function mpfr_div(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // --- Specials ---------------------------------------------------------
  // (1) NaN propagation.
  if (a.kind === 'nan' || b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) ±Inf handling.
  if (a.kind === 'inf') {
    if (b.kind === 'inf') {
      // Inf / Inf — indeterminate. Mirrors mpfr/src/div.c L794-L798.
      return { value: NAN_VALUE, ternary: 0 };
    }
    // a is Inf, b is finite (zero or normal). Result: sign-product Inf.
    const sign = multSign(a.sign, b.sign);
    return {
      value: sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }
  if (b.kind === 'inf') {
    // a is finite (zero or normal), b is Inf. Result: sign-product zero.
    // Mirrors mpfr/src/div.c L805-L808.
    const sign = multSign(a.sign, b.sign);
    return {
      value: sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (3) Divide-by-zero: b is ±0 and a is not Inf (ruled out above).
  if (b.kind === 'zero') {
    if (a.kind === 'zero') {
      // 0 / 0 — indeterminate. Mirrors mpfr/src/div.c L812-L815.
      return { value: NAN_VALUE, ternary: 0 };
    }
    // a is normal, b is ±0. Result: sign-product Inf (divbyzero).
    // Mirrors mpfr/src/div.c L817-L823.
    const sign = multSign(a.sign, b.sign);
    return {
      value: sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // (4) Numerator is ±0, denominator is finite-nonzero (normal).
  if (a.kind === 'zero') {
    // 0 / finite-nonzero. Result: sign-product zero, ternary 0.
    // Mirrors mpfr/src/div.c L827-L829.
    const sign = multSign(a.sign, b.sign);
    return {
      value: sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (5) normal / normal — the algebraic core.
  if (a.kind !== 'normal' || b.kind !== 'normal') {
    // Defensive: unreachable. All other kinds dispatched above.
    throw new MPFRError(
      'EPREC',
      `mpfr_div: unexpected kinds a=${a.kind} b=${b.kind} at normal-normal branch`,
    );
  }
  return divNormalNormal(a, b, prec, rnd);
}
