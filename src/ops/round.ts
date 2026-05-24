/**
 * ops/round.ts — pure-TS port of MPFR's `mpfr_round`.
 *
 * Round an {@link MPFR} value to the nearest integer **representable at
 * the target precision**, with ties away from zero (RNDNA), returning
 * the canonical `{value, ternary}` shape from src/core.ts.
 *
 * Critical subtlety: `mpfr_round(4.7) at prec=2 = 4`, NOT 5. The
 * function rounds x to the nearest **prec-representable integer**, NOT
 * "round x to nearest integer then fit at prec". Since 5 is not
 * representable at prec=2 (the prec-2 integers near 4.7 are 4 and 6),
 * the closer of {4, 6} wins — 4 (distance 0.7) beats 6 (distance 1.3).
 *
 * For ties at the rounding boundary (e.g. round(5) prec=2: candidates
 * 4 and 6, each distance 1), RNDNA picks the one further from zero — 6
 * for round(+5), -6 for round(-5).
 *
 * The only place this differs from a "naive integer-round-then-pack" is
 * when |x| has more significant bits than fit at the target prec. For
 * small magnitudes (|x| < 2^prec) the two interpretations coincide.
 *
 * C signature
 * -----------
 *
 *   int mpfr_round(mpfr_t r, mpfr_srcptr u);
 *
 *   mpfr_round defers to mpfr_rint(r, u, MPFR_RNDNA) — mpfr/src/rint.c
 *   L317–L320. mpfr_rint's main loop (L99–L303) implements the unified
 *   "round x to nearest prec-representable integer in the rule's
 *   direction" semantics via mantissa-bit masking + an optional
 *   ulp-increment based on `rnd_away`.
 *
 * TS signature
 * ------------
 *
 *   mpfr_round(x: MPFR, prec: bigint): Result;
 *
 *   - takes `prec` as an explicit positional argument;
 *   - no rounding-mode parameter (the direction is implicit — RNDNA);
 *   - returns the immutable {@link Result} from src/core.ts.
 *
 * Algorithm
 * ---------
 *
 * Specials propagate: NaN→NaN, ±Inf→±Inf (sign preserved), ±0→±0
 * (sign preserved per MPFR_SET_SAME_SIGN, mpfr/src/rint.c L62).
 *
 * For normal x:
 *
 *   1. |x| < 1 (`x.exp <= 0`): the natural rounded value would be a
 *      fraction, but mpfr_rint forces an integer here. Result is 0
 *      (with x's sign — signed zero!) if |x| < 0.5, or sign(x) if
 *      |x| > 0.5, or sign(x) (ties away from zero) if |x| == 0.5
 *      exactly. Ref: mpfr/src/rint.c L80–L98.
 *
 *   2. |x| >= 1 (`x.exp >= 1`): use the unified algorithm from
 *      mpfr/src/rint.c L99–L303 in bigint form. See `roundIntegralRNDNA`
 *      below.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/rint.c L317–L320 — wrapper.
 *   - mpfr/src/rint.c L35–L304 — mpfr_rint engine; the L80–L98 (|u|<1)
 *     and L99–L303 (|u|>=1) branches are the two regimes.
 *   - mpfr/src/rint.c L67–L72 — `rnd_away` selection. For RNDNA the
 *     value is `-1` (decide later from dropped bits).
 *   - mpfr/src/rint.c L237–L277 — RNDN/RNDNA tie decision in the
 *     |u|>=1 path. RNDNA always rounds away on a tie.
 *   - mpfr/tests/trint.c L112–L132 — coverage tests including
 *     round(2.5) at prec=2 → 3 (RNDNA tie-away).
 *   - src/ops/trunc.ts, src/ops/ceil.ts, src/ops/floor.ts — siblings
 *     with shared algorithm scaffolding.
 *   - src/core.ts — locked types.
 */

import type { MPFR, Result, Sign } from '../core.ts';
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

function validatePrec(prec: bigint): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
}

/**
 * The |x|>=1 path. Implements the unified mpfr_rint algorithm for
 * RNDNA: compute the truncated mantissa in r's prec frame, then add
 * one ulp iff the dropped bits indicate "should round away" under
 * RNDNA's "nearest, ties away from zero" rule.
 *
 * Returns `{mant, exp, sign, ternary}` for assembly into a Result.
 *
 * Ref: mpfr/src/rint.c L99–L303, in single-bigint form.
 */
function roundIntegralRNDNA(
  x: MPFR,
  prec: bigint,
): { mant: bigint; exp: bigint; sign: Sign; ternary: -1 | 0 | 1 } {
  const xExp = x.exp;
  const xPrec = x.prec;
  const xMant = x.mant;
  const sign: Sign = x.sign;

  // The result's natural exponent is x.exp (the integer-part bit length
  // of |x|, since x.exp = bitLength(integerPart(|x|)) when |x| >= 1).
  // Regime A (xExp >= prec): drop the low (xExp - prec) bits of the
  // integer part. Plus all fractional bits of x.
  // Regime B (xExp < prec): the integer part fits in prec; we pad it.

  let truncMant: bigint;
  let droppedTopBitIsSet: boolean; // first bit dropped past the new prec
  let restDroppedNonZero: boolean; // remaining dropped bits are non-zero

  if (xExp >= prec) {
    // Regime A: drop (xExp - prec) low bits of intAbs, plus all
    // fractional bits.
    //   intAbs = xMant >> (xPrec - xExp)            if xExp < xPrec
    //   intAbs = xMant << (xExp - xPrec)            if xExp >= xPrec
    // Concretely, the result mantissa is the top `prec` bits of
    // intAbs, i.e. shift xMant right by (xPrec - prec).
    //   - if xPrec >= prec: this is just (xMant >> (xPrec - prec))
    //     and the dropped bits include both integer-part low bits and
    //     all fractional bits.
    //   - if xPrec < prec: this is (xMant << (prec - xPrec)) and we
    //     shift left to pad. But xExp >= prec > xPrec means xExp > xPrec
    //     i.e. x is already an integer with trailing zero bits beyond
    //     the mantissa frame. Then the integer-part bits below prec
    //     are all zero — no dropped bits.
    if (xPrec >= prec) {
      const shift = xPrec - prec;
      truncMant = xMant >> shift;
      // Dropped bits: low `shift` bits of xMant. The highest dropped
      // bit is at position (shift - 1) i.e. xMant's bit (shift-1).
      if (shift === 0n) {
        droppedTopBitIsSet = false;
        restDroppedNonZero = false;
      } else {
        const highDroppedMask = 1n << (shift - 1n);
        droppedTopBitIsSet = (xMant & highDroppedMask) !== 0n;
        const restMask = highDroppedMask - 1n;
        restDroppedNonZero = (xMant & restMask) !== 0n;
      }
    } else {
      // xPrec < prec but xExp >= prec means xExp > xPrec — x is already
      // an integer with the mantissa frame leaving low bits implicit
      // zero. To get the top `prec` bits of intAbs, shift xMant left
      // by (prec - xPrec). No dropped bits.
      truncMant = xMant << (prec - xPrec);
      droppedTopBitIsSet = false;
      restDroppedNonZero = false;
    }
  } else {
    // Regime B (xExp < prec): the integer part of |x| has xExp bits;
    // we pad to prec bits. The "dropped" bits here are the fractional
    // bits of x (the low (xPrec - xExp) bits of xMant), if any.
    // Compute the integer part of |x| as a bigint of bitLength xExp:
    //   intAbs = xMant >> (xPrec - xExp)    when xPrec > xExp
    //   intAbs = xMant << (xExp - xPrec)    when xPrec <= xExp (here
    //                                       impossible since xExp < prec
    //                                       and we'd need xPrec <= xExp
    //                                       < prec, then intAbs has
    //                                       xExp bits and we pad to
    //                                       prec by shifting left).
    let intAbs: bigint;
    let fracBits: bigint;
    let hasFractionalBits: boolean;
    if (xPrec > xExp) {
      const fracBitsCount = xPrec - xExp;
      intAbs = xMant >> fracBitsCount;
      fracBits = xMant & ((1n << fracBitsCount) - 1n);
      hasFractionalBits = fracBits !== 0n;
      // For the round-step at prec, the "first dropped bit" is the MSB
      // of fracBits (bit at position fracBitsCount-1), and the rest
      // are the lower bits of fracBits.
      if (hasFractionalBits) {
        const highDroppedMask = 1n << (fracBitsCount - 1n);
        droppedTopBitIsSet = (fracBits & highDroppedMask) !== 0n;
        restDroppedNonZero = (fracBits & (highDroppedMask - 1n)) !== 0n;
      } else {
        droppedTopBitIsSet = false;
        restDroppedNonZero = false;
      }
    } else {
      // xPrec <= xExp < prec: x is an integer (no fractional bits) with
      // trailing zero bits implicit in the frame. intAbs has xExp bits
      // (bitLength of mant shifted up by xExp-xPrec).
      intAbs = xMant << (xExp - xPrec);
      droppedTopBitIsSet = false;
      restDroppedNonZero = false;
    }
    // Pad intAbs from xExp bits to prec bits.
    truncMant = intAbs << (prec - xExp);
  }

  // x is an exact integer that fits at prec iff no bit was dropped.
  const exact = !droppedTopBitIsSet && !restDroppedNonZero;
  if (exact) {
    return {
      mant: truncMant,
      exp: xExp,
      sign,
      ternary: 0,
    };
  }

  // RNDNA increment rule: increment iff the "value of the dropped bits"
  // is >= half-ulp of the result. The half-ulp boundary is the highest
  // dropped bit (`droppedTopBitIsSet`). If droppedTopBitIsSet:
  //   - if restDroppedNonZero: dropped > half → increment (round up).
  //   - else: dropped == half → tie. RNDNA always rounds away from
  //     zero → increment.
  // If !droppedTopBitIsSet:
  //   - dropped < half → don't increment.
  //   - But droppedTopBitIsSet=false AND !exact means restDroppedNonZero
  //     is true, i.e. dropped > 0 but < half. Don't increment.
  const increment = droppedTopBitIsSet;

  let mant: bigint;
  let exp: bigint;
  let ternary: -1 | 0 | 1;
  if (!increment) {
    // Truncating: rounded magnitude < exact magnitude.
    mant = truncMant;
    exp = xExp;
    ternary = sign === 1 ? -1 : 1;
  } else {
    // Increment by 1 ulp-of-the-integer. In Regime A (xExp >= prec)
    // the result's integer ulp is the mantissa's LSB (= 1). In Regime
    // B (xExp < prec) the result's integer ulp corresponds to mantissa
    // step 2^(prec - xExp) — we padded the integer with that many zero
    // bits, so to add 1 in value we add 2^(prec - xExp) in mantissa.
    const ulp = xExp >= prec ? 1n : 1n << (prec - xExp);
    const incremented = truncMant + ulp;
    const upperBound = 1n << prec;
    if (incremented === upperBound) {
      // Carry-out: mantissa overflows; renormalise.
      mant = upperBound >> 1n;
      exp = xExp + 1n;
    } else {
      mant = incremented;
      exp = xExp;
    }
    ternary = sign === 1 ? 1 : -1;
  }
  return { mant, exp, sign, ternary };
}

/**
 * Round an MPFR value to the nearest prec-representable integer
 * (ties away from zero) at the target precision.
 *
 * @mpfrName mpfr_round
 *
 * @param x     the operand. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 *
 * @returns     `{value, ternary}`. Value passes `validate()` without
 *              post-processing. Ternary is `0` iff the result equals x
 *              exactly (x was already a prec-representable integer).
 *
 * @throws {MPFRError} `EPREC` on bad precision. NaN/Inf is NOT an error.
 *
 * @example
 *   round(setD(0.5, 53n, 'RNDN').value, 53n);
 *     // → {value: 1, ternary: +1}  (ties away from zero)
 *   round(setD(2.5, 53n, 'RNDN').value, 53n);
 *     // → {value: 3, ternary: +1}  (RNDNA, not ties-to-even)
 *   round(setD(4.7, 53n, 'RNDN').value, 2n);
 *     // → {value: 4, ternary: -1}  (prec=2 reps near 4.7: 4 and 6;
 *     //                              4 closer; NOT 5 → 6 via two-step)
 */
export function mpfr_round(x: MPFR, prec: bigint): Result {
  validatePrec(prec);

  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }
  if (x.kind === 'inf') {
    return {
      value: x.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }
  if (x.kind === 'zero') {
    return {
      value: x.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_round: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  // --- |x| < 1 branch (x.exp <= 0): force result to {0, ±1}. ----------
  if (x.exp <= 0n) {
    // RNDNA: |x| < 0.5 → 0 (with x's sign); |x| > 0.5 → ±1;
    // |x| == 0.5 (exact tie) → ±1 (away from zero).
    // |x| < 0.5 iff x.exp < 0. |x| == 0.5 iff x.exp == 0 AND mant is
    // MSB-only (the value's only set bit is the leading 1).
    // |x| > 0.5 iff x.exp == 0 AND mant has any bit besides the MSB.
    const isAtLeastHalf =
      x.exp === 0n; // exp==0 → |x| in [0.5, 1); exp<0 → |x| < 0.5
    if (!isAtLeastHalf) {
      // |x| < 0.5 → round to 0 with sign preserved.
      const value: MPFR =
        x.sign === 1
          ? { kind: 'zero', sign: 1, prec, exp: 0n, mant: 0n }
          : { kind: 'zero', sign: -1, prec, exp: 0n, mant: 0n };
      // ternary = sign(0 - x). x > 0 → ternary -1; x < 0 → ternary +1.
      return { value, ternary: x.sign === 1 ? -1 : 1 };
    }
    // |x| in [0.5, 1) — RNDNA rounds to ±1 (tie-away at exactly 0.5,
    // round-up above 0.5).
    const value: MPFR = {
      kind: 'normal',
      sign: x.sign,
      prec,
      exp: 1n,
      mant: 1n << (prec - 1n),
    };
    // Is x exactly ±1? No (x.exp == 0 means |x| < 1). So ternary is
    // sign(±1 - x): if x.sign=1, sign(1 - x) = +1 iff x < 1, always
    // here. If x.sign=-1, sign(-1 - x) = sign(-1 + |x|) which is -1
    // since |x| < 1. So:
    return { value, ternary: x.sign === 1 ? 1 : -1 };
  }

  // --- |x| >= 1 branch (x.exp >= 1). ----------------------------------
  const r = roundIntegralRNDNA(x, prec);
  const value: MPFR = {
    kind: 'normal',
    sign: r.sign,
    prec,
    exp: r.exp,
    mant: r.mant,
  };
  return { value, ternary: r.ternary };
}
