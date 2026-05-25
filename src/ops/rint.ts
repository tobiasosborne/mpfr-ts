/**
 * ops/rint.ts -- pure-TS port of MPFR's `mpfr_rint`.
 *
 * Round an {@link MPFR} value to the nearest prec-representable integer
 * in the requested rounding mode, returning the canonical
 * `{value, ternary}` shape from src/core.ts.
 *
 * `mpfr_rint` is the general round-to-integer dispatcher; the C
 * wrappers `mpfr_trunc`, `mpfr_floor`, `mpfr_ceil`, `mpfr_round`
 * (mpfr/src/rint.c L317-L344) defer to it with a fixed `rnd_mode`.
 * The TS side already ships those four wrappers as standalone bigint
 * ports; this file is the *parent* dispatcher with all five rounding
 * modes selectable.
 *
 * Critical subtlety: `mpfr_rint(u, prec, 'RNDN')` is **round to nearest,
 * ties to even** (the IEEE-754 default). It differs from `mpfr_round(u,
 * prec)` which is RNDNA (legacy MPFR "ties away from zero"). At a
 * halfway like `5.0` rounded to prec=2 the candidates are 4 and 6, both
 * at distance 1 -- RNDN picks 4 (mantissa `0b10`, even LSB), RNDNA picks
 * 6 (away from zero).
 *
 * C signature
 * -----------
 *
 *   int mpfr_rint(mpfr_ptr r, mpfr_srcptr u, mpfr_rnd_t rnd_mode);
 *
 * Ref: mpfr/src/rint.c L35-L304.
 *
 * TS signature
 * ------------
 *
 *   mpfr_rint(u: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm: split dispatch
 * -------------------------
 *
 * For four of the five rounding modes the result equals the sibling
 * wrapper's output at the target prec (mpfr/src/rint.c L317-L344
 * codifies this -- each wrapper is `mpfr_rint(r, u, FIXED_RND)`):
 *
 *   - RNDZ -> mpfr_trunc(u, prec).
 *   - RNDU -> mpfr_ceil(u, prec).
 *   - RNDD -> mpfr_floor(u, prec).
 *   - RNDA -> mpfr_ceil(u, prec) if u.sign > 0 else mpfr_floor(u, prec).
 *           Ref: mpfr/src/rint.c L67-L71 -- `rnd_away` for RNDA is `1`
 *           (always round away), which matches ceil for positives and
 *           floor for negatives.
 *
 * For RNDN we cannot delegate to `mpfr_round`: that wrapper applies
 * RNDNA (ties away from zero) per mpfr/src/rint.c L317-L320. The RNDN
 * branch is implemented inline with bigint logic that mirrors the
 * `roundIntegralRNDNA` shape from src/ops/round.ts but flips the tie
 * decision: at an exact halfway the truncated mantissa's LSB drives
 * the choice -- LSB=0 (even) keeps the truncation, LSB=1 (odd)
 * increments to make the result mantissa even.
 *
 * Naive two-pass is wrong
 * -----------------------
 *
 * One might think "round u to nearest integer at u.prec, then mpfr_set
 * that integer to target prec in rnd" would work. It doesn't. Example:
 * u=5.4, target prec=2, rnd=RNDN. The C path rounds u directly at
 * prec=2 -- dropped bits "1.01100..." past truncated "10" mean round
 * bit = 1 with nonzero rest, so round up to 6. The naive two-pass
 * rounds u to nearest integer at u.prec first (gives 5), then rounds 5
 * to prec=2 RNDN (candidates 4 and 6 are both at distance 1 -- a tie;
 * ties-to-even picks 4 since `0b10` is even). 6 != 4, so the two-pass
 * is incorrect at low target precs. The fix is to track ALL dropped
 * bits below the target-prec boundary, which is what the per-mode
 * integral helpers in floor/ceil/trunc/round do; this port follows the
 * same shape for the RNDN branch.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/rint.c L35-L304 -- C reference body.
 *   - mpfr/src/rint.c L67-L72 -- rnd_away encoding for each rnd.
 *   - mpfr/src/rint.c L80-L98 -- |u|<1 branch.
 *   - mpfr/src/rint.c L99-L303 -- |u|>=1 general path.
 *   - mpfr/src/rint.c L155-L190, L237-L277 -- RNDN tie decisions.
 *   - src/ops/trunc.ts, ceil.ts, floor.ts, round.ts -- sibling
 *     wrappers; the first three are reused here, round.ts is the
 *     algorithmic template for the RNDN branch (different tie rule).
 *   - src/core.ts -- locked types.
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
import { mpfr_ceil } from './ceil.ts';
import { mpfr_floor } from './floor.ts';
import { mpfr_trunc } from './trunc.ts';

const VALID_RND: ReadonlySet<RoundingMode> = new Set<RoundingMode>([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
]);

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
  if (!VALID_RND.has(rnd)) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * RNDN |x|>=1 integer-rounding path. Computes the prec-representable
 * integer nearest |x|, ties to even (mantissa LSB=0). Returns the
 * mantissa, exponent, sign, and ternary for assembly into a Result.
 *
 * Mirrors src/ops/round.ts `roundIntegralRNDNA` in structure but
 * differs only in the tie-decision: at an exact halfway, the C side
 * (mpfr/src/rint.c L158-L184 for the ui>rn limb regime, L243-L271 for
 * the ui<=rn regime) inspects `(rp[0] & (MPFR_LIMB_ONE << sh)) == 0`
 * -- i.e. "is the truncated mantissa's LSB at the target prec even" --
 * and if so keeps the truncation; otherwise rounds away.
 *
 * Refs:
 *   - mpfr/src/rint.c L99-L303 -- C reference body.
 *   - mpfr/src/rint.c L158-L184 -- RNDN tie decision in regime A
 *     (target prec smaller than integer-part width).
 *   - mpfr/src/rint.c L243-L271 -- RNDN tie decision in regime B
 *     (integer part fits in target prec).
 */
function roundIntegralRNDN(
  x: MPFR,
  prec: bigint,
): { mant: bigint; exp: bigint; sign: Sign; ternary: -1 | 0 | 1 } {
  const xExp = x.exp;
  const xPrec = x.prec;
  const xMant = x.mant;
  const sign: Sign = x.sign;

  // Compute the truncated-toward-zero mantissa at target prec, and
  // capture the first dropped bit and whether anything beyond it is
  // nonzero. Mirrors src/ops/round.ts and the C `rp[0] & MASK` /
  // `up[...]` inspections at rint.c L158-L189 and L246-L276.
  let truncMant: bigint;
  let droppedTopBitIsSet: boolean;
  let restDroppedNonZero: boolean;

  if (xExp >= prec) {
    if (xPrec >= prec) {
      const shift = xPrec - prec;
      truncMant = xMant >> shift;
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
      // xPrec < prec but xExp >= prec implies xExp > xPrec, i.e. x is
      // an integer with implicit trailing zeros. No dropped bits.
      // Ref: mpfr/src/rint.c L123-L128 -- "u is an integer,
      // representable or not in r" via `(exp-1)/GMP_NUMB_BITS >= un`.
      truncMant = xMant << (prec - xPrec);
      droppedTopBitIsSet = false;
      restDroppedNonZero = false;
    }
  } else {
    // Regime B: integer part has xExp bits, pad to prec; dropped bits
    // are the fractional bits of x (low xPrec-xExp bits of xMant) if
    // xPrec > xExp; otherwise x is an integer with implicit trailing
    // zeros and nothing is dropped.
    // Ref: mpfr/src/rint.c L207-L235 -- ui<=rn regime in C.
    if (xPrec > xExp) {
      const fracBitsCount = xPrec - xExp;
      const intAbs = xMant >> fracBitsCount;
      const fracBits = xMant & ((1n << fracBitsCount) - 1n);
      if (fracBits === 0n) {
        droppedTopBitIsSet = false;
        restDroppedNonZero = false;
      } else {
        const highDroppedMask = 1n << (fracBitsCount - 1n);
        droppedTopBitIsSet = (fracBits & highDroppedMask) !== 0n;
        restDroppedNonZero = (fracBits & (highDroppedMask - 1n)) !== 0n;
      }
      truncMant = intAbs << (prec - xExp);
    } else {
      truncMant = xMant << (prec - xPrec);
      droppedTopBitIsSet = false;
      restDroppedNonZero = false;
    }
  }

  const exact = !droppedTopBitIsSet && !restDroppedNonZero;
  if (exact) {
    return { mant: truncMant, exp: xExp, sign, ternary: 0 };
  }

  // The integer-step ulp at the result prec, both in mantissa-space and
  // as the bit position whose value distinguishes adjacent representable
  // integers. In Regime A (xExp >= prec) only every 2^(xExp-prec)'th
  // integer is representable, but mantissa step 1 carries us between
  // them, so the "integer LSB" lives at mantissa bit 0. In Regime B
  // (xExp < prec) every integer in [2^(xExp-1), 2^xExp) is representable
  // and adjacent integers correspond to mantissa step 2^(prec - xExp),
  // so the "integer LSB" of truncMant lives at bit position (prec - xExp).
  // Ref: mpfr/src/rint.c L243-L244 -- the C test is
  //   `(rp[0] & (MPFR_LIMB_ONE << sh)) == 0`
  // where `sh` is set so that bit `sh` is exactly this "integer-LSB"
  // position in limb-space; the bigint equivalent is `truncMant & ulp`.
  const ulp = xExp >= prec ? 1n : 1n << (prec - xExp);

  // RNDN tie-to-even decision: if dropped > half-ulp (round bit set and
  // any rest bit set), round up. If dropped < half-ulp (round bit clear,
  // possibly rest nonzero), round down. If exact half (round bit set,
  // rest zero), round to even: increment iff the integer-LSB of
  // truncMant is 1 (odd) -- this makes the new LSB 0 (even) after the
  // carry, mirroring the C "halfway cases rounded away from zero" path
  // at rint.c L186-L189 / L272-L276.
  let increment: boolean;
  if (droppedTopBitIsSet) {
    if (restDroppedNonZero) {
      increment = true; // strictly more than half-ulp
    } else {
      // exact halfway: ties-to-even on the integer LSB.
      increment = (truncMant & ulp) !== 0n;
    }
  } else {
    increment = false; // strictly less than half-ulp
  }

  let mant: bigint;
  let exp: bigint;
  let ternary: -1 | 0 | 1;
  if (!increment) {
    mant = truncMant;
    exp = xExp;
    // Magnitude truncated; rounded magnitude < exact magnitude.
    ternary = sign === 1 ? -1 : 1;
  } else {
    const incremented = truncMant + ulp;
    const upperBound = 1n << prec;
    if (incremented === upperBound) {
      // Carry-out: renormalise.
      // Ref: mpfr/src/rint.c L290-L300 -- the mpn_add_1 carry case
      // bumps exp by 1 and sets mant high bit.
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
 * Round u to the nearest prec-representable integer in mode `rnd`.
 *
 * @mpfrName mpfr_rint
 *
 * @param u    operand (any kind).
 * @param prec output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd  one of the five RoundingMode values.
 *
 * @returns `{value, ternary}` -- the rounded value and the ternary
 *          flag (sign of rounded - exact).
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on unknown rnd.
 *
 * @example
 *   mpfr_rint(setD(0.5, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // -> {value: +0, ternary: -1}   (RNDN ties-to-even; 0 is even)
 *   mpfr_rint(setD(2.5, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // -> {value: 2, ternary: -1}    (RNDN ties-to-even; 2 even)
 *   mpfr_rint(setD(3.5, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // -> {value: 4, ternary: +1}    (RNDN ties-to-even; 4 even)
 *   mpfr_rint(setD(-2.7, 53n, 'RNDN').value, 53n, 'RNDA');
 *     // -> {value: -3, ternary: -1}   (RNDA away-from-zero, ties away)
 *
 * Ref: mpfr/src/rint.c L35-L304 -- C reference body.
 */
export function mpfr_rint(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // ----------------------- singular dispatch -------------------------
  // Ref: mpfr/src/rint.c L42-L61 -- NaN -> NaN, +-Inf -> +-Inf (sign
  // preserved, ternary 0), +-0 -> +-0 (sign preserved, ternary 0).
  if (u.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }
  if (u.kind === 'inf') {
    return {
      value: u.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }
  if (u.kind === 'zero') {
    return {
      value: u.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }
  if (u.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_rint: unexpected kind ${String((u as { kind?: unknown }).kind)}`,
    );
  }

  // ---------------- delegation for RNDZ/RNDU/RNDD/RNDA ---------------
  // Each sibling wrapper is defined in C as `mpfr_rint(r, u, FIXED)`
  // (mpfr/src/rint.c L317-L344). RNDA isn't itself a wrapper but the
  // semantics match ceil-or-floor by sign (mpfr/src/rint.c L67-L71:
  // rnd_away for RNDA is `1`, i.e. round magnitude up, which is ceil
  // for positives and floor for negatives).
  if (rnd === 'RNDZ') {
    return mpfr_trunc(u, prec);
  }
  if (rnd === 'RNDU') {
    return mpfr_ceil(u, prec);
  }
  if (rnd === 'RNDD') {
    return mpfr_floor(u, prec);
  }
  if (rnd === 'RNDA') {
    return u.sign === 1 ? mpfr_ceil(u, prec) : mpfr_floor(u, prec);
  }

  // ----------------------- RNDN: ties to even ------------------------
  // |u| < 1 branch -- Ref: mpfr/src/rint.c L80-L98. For RNDN, the C
  // condition at L83-L86 reduces to: round away iff (exp==0) AND
  // (rnd_mode == RNDNA OR !powerof2(u)). With rnd_mode=RNDN, the
  // powerof2 check is the tie test for |u|=0.5; if !powerof2 then
  // |u|>0.5 and we round to +-1.
  if (u.exp <= 0n) {
    // exp<0 -> |u|<0.5 -> round to 0 with sign preserved (ties-to-even
    // not triggered; magnitude strictly below half-ulp).
    if (u.exp < 0n) {
      const value: MPFR =
        u.sign === 1
          ? { kind: 'zero', sign: 1, prec, exp: 0n, mant: 0n }
          : { kind: 'zero', sign: -1, prec, exp: 0n, mant: 0n };
      return { value, ternary: u.sign === 1 ? -1 : 1 };
    }
    // exp == 0 -> |u| in [0.5, 1).
    //   * |u| == 0.5 exactly (u.mant has only its MSB set): RNDN tie;
    //     round to nearest even integer -> 0.
    //   * |u| > 0.5: round to +-1.
    const isHalf = u.mant === 1n << (u.prec - 1n);
    if (isHalf) {
      // |u| == 0.5 -- tie, even is 0.
      // Ref: mpfr/src/rint.c L86 -- `!mpfr_powerof2_raw(u)` is false
      // here, so the C falls through to the else (round to zero).
      const value: MPFR =
        u.sign === 1
          ? { kind: 'zero', sign: 1, prec, exp: 0n, mant: 0n }
          : { kind: 'zero', sign: -1, prec, exp: 0n, mant: 0n };
      return { value, ternary: u.sign === 1 ? -1 : 1 };
    }
    // |u| > 0.5 -- round magnitude up to 1, sign preserved.
    const value: MPFR = {
      kind: 'normal',
      sign: u.sign,
      prec,
      exp: 1n,
      mant: 1n << (prec - 1n),
    };
    return { value, ternary: u.sign === 1 ? 1 : -1 };
  }

  // |u| >= 1 branch -- general bigint algorithm.
  // Ref: mpfr/src/rint.c L99-L303.
  const r = roundIntegralRNDN(u, prec);
  const value: MPFR = {
    kind: 'normal',
    sign: r.sign,
    prec,
    exp: r.exp,
    mant: r.mant,
  };
  return { value, ternary: r.ternary };
}
