/**
 * ops/min.ts — pure-TS port of MPFR's `mpfr_min`.
 *
 * Return the smaller of two {@link MPFR} operands, rounded to the target
 * precision per the rounding mode. Sibling to max.ts; the two share
 * structure but differ in their compare-result branches and signed-zero
 * tie-breaks.
 *
 * C signature
 * -----------
 *
 *   int mpfr_min(mpfr_t rop, mpfr_srcptr x, mpfr_srcptr y, mpfr_rnd_t rnd);
 *
 *   Returns the ternary. Sets `rop` to `min(x, y)` per IEEE 754-2008
 *   `minNum` semantics: NaN propagates only if both are NaN; otherwise
 *   the non-NaN operand wins. See mpfr/src/minmax.c L31–L57.
 *
 *   IEEE 754-2008 vs 754-2019: the 2008 `minNum` returns the non-NaN
 *   operand when one is NaN; the 2019 `minimum` propagates NaN. MPFR
 *   follows the 2008 contract (the comment on mpfr/src/minmax.c L25
 *   spells out the cmp-based dispatch). Do NOT replace with a
 *   NaN-propagating variant.
 *
 *   Signed-zero handling: `min(+0, -0)` and `min(-0, +0)` both return
 *   `-0`. MPFR's source explicitly checks the both-zero case
 *   (mpfr/src/minmax.c L45–L51), returning the negative operand if
 *   either is negative — this matches IEEE 754 `minNum` which orders
 *   signed zero as `-0 < +0`. The generic `mpfr_cmp` branch below the
 *   special-case dispatch would return 0 for the zero-zero pair,
 *   which would mis-route to whichever operand happens to be `x`; the
 *   pre-test is therefore load-bearing for correctness.
 *
 * TS signature
 * ------------
 *
 *   mpfr_min(a: MPFR, b: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   Both operands are immutable. The result carries the value of the
 *   smaller operand, rounded to `prec` per `rnd`.
 *
 * Algorithm
 * ---------
 *
 *   1. Both NaN → NAN_VALUE. (mpfr/src/minmax.c L36–L40.)
 *   2. One NaN → the non-NaN operand, rounded to prec via the standard
 *      mpfr_set-like dispatch. (mpfr/src/minmax.c L41–L44.)
 *   3. Both zero (any sign) → `-0` if either is negative, else `+0`.
 *      (mpfr/src/minmax.c L45–L51.)
 *   4. Otherwise: dispatch on `compareMPFR(a, b)`:
 *        - `<= 0` → return `a`, rounded to prec.
 *        - `> 0`  → return `b`, rounded to prec.
 *      (mpfr/src/minmax.c L53–L56.)
 *
 * Rounding to prec
 * ----------------
 *
 * Once the winning operand is chosen, we apply the same prec-conversion
 * the C reference does via `mpfr_set` (which mpfr/src/minmax.c L42, L48,
 * L50, L54, L56 all call): lossless left-pad when prec >= operand.prec,
 * substrate `roundMantissa` when prec < operand.prec. The rounding-mode
 * sign is the operand's own sign (mpfr_set, not setsign — we preserve
 * the existing sign, not impose a new one). Specials (zero/inf/nan)
 * never require rounding; only the `normal` branch does.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/minmax.c L31–L57 — the C reference.
 *   - mpfr/src/cmp.c L32–L98 — `mpfr_cmp3`, called by minmax via the
 *     `mpfr_cmp` macro. Note: `mpfr_cmp` returns 0 for `±0 vs ±0`
 *     regardless of sign, which is why the all-zero pre-test in step 3
 *     above is required.
 *   - src/internal/mpfr/cmp_raw.ts — substrate `compareMPFR`; returns
 *     `null` on NaN (unordered) and `0` on a both-zero pair (signed
 *     zero is NOT ordered by cmp).
 *   - src/ops/abs.ts / src/ops/neg.ts — siblings whose docstring
 *     explains the "round mantissa to prec with the operand's own
 *     sign" pattern used in `roundToPrec` below.
 *   - src/internal/mpfr/round_raw.ts — substrate rounding primitive.
 *   - src/core.ts — locked MPFR / RoundingMode / Result / Sign types.
 *   - CLAUDE.md "Hallucination-risk callouts" — Signed zero is real
 *     (min(+0, -0) = -0); NaN ≠ NaN (NaN x non-NaN → non-NaN, per
 *     IEEE 754-2008); rounding-mode count is FIVE.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negZero,
  posZero,
} from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Validate the public-boundary scalar arguments.
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
 * Round `x` to the target precision `prec` per `rnd`, preserving x's
 * kind and sign. This is the in-library analog of MPFR's `mpfr_set`
 * (mpfr/src/set.c L25–L64), specialised for the case where we already
 * know the source value and just need to refit it.
 *
 * Specials (zero/inf/nan) pass through with the new prec attached;
 * normal values either lossless-pad (prec >= x.prec) or round via the
 * substrate primitive. Ternary is 0 for every special and for the
 * lossless-pad case; only the `prec < x.prec` normal branch can return
 * a nonzero ternary.
 *
 * Used by both min.ts and max.ts to refit the winning operand.
 */
function roundToPrec(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }
  if (x.kind === 'inf') {
    return {
      value: { kind: 'inf', sign: x.sign, prec, exp: 0n, mant: 0n },
      ternary: 0,
    };
  }
  if (x.kind === 'zero') {
    return {
      value: { kind: 'zero', sign: x.sign, prec, exp: 0n, mant: 0n },
      ternary: 0,
    };
  }
  if (prec >= x.prec) {
    const padShift = prec - x.prec;
    return {
      value: {
        kind: 'normal',
        sign: x.sign,
        prec,
        exp: x.exp,
        mant: x.mant << padShift,
      },
      ternary: 0,
    };
  }
  // Lossy: round to fewer bits via the substrate. Pass the operand's own
  // sign for the RNDU/RNDD direction lookup, matching mpfr_set4's signb
  // contract (mpfr/src/set.c L58).
  const { mant, exp, ternary } = roundMantissa(
    x.mant,
    x.prec,
    x.exp,
    prec,
    x.sign,
    rnd,
  );
  return {
    value: { kind: 'normal', sign: x.sign, prec, exp, mant },
    ternary,
  };
}

/**
 * Return `min(a, b)` rounded to the target precision.
 *
 * Per IEEE 754-2008 `minNum`: when one operand is NaN the other is
 * returned; both NaN → NaN; signed zeros order as `-0 < +0`.
 *
 * @mpfrName mpfr_min
 *
 * @param a     left operand. Any kind.
 * @param b     right operand. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}` — the smaller operand rounded to prec.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf inputs are NOT errors.
 *
 * @example
 *   min(setD(1, 53n, 'RNDN').value, setD(2, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → 1, ternary 0
 *   min(NAN_VALUE, setD(0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → 0, ternary 0  (IEEE 754-2008 minNum: NaN ignored)
 *   min(posZero(53n), negZero(53n), 53n, 'RNDN');
 *     // → -0, ternary 0  (signed-zero ordering)
 */
export function mpfr_min(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  const aNan = a.kind === 'nan';
  const bNan = b.kind === 'nan';

  // (1) Both NaN → NaN. Ref: mpfr/src/minmax.c L36–L40.
  if (aNan && bNan) {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) Exactly one NaN — return the other. The non-NaN operand still
  // gets rounded to prec via the standard set-like dispatch. Ref:
  // mpfr/src/minmax.c L41–L44.
  if (aNan) return roundToPrec(b, prec, rnd);
  if (bNan) return roundToPrec(a, prec, rnd);

  // (3) Both zero — IEEE 754-2008 minNum orders `-0 < +0`. Return the
  // negative one if either is negative, else +0. Ref:
  // mpfr/src/minmax.c L45–L51.
  if (a.kind === 'zero' && b.kind === 'zero') {
    // C source: `if (MPFR_IS_NEG(x)) return mpfr_set(z, x, ...); else
    // return mpfr_set(z, y, ...);` — so when `a` is the negative one
    // we return `a`; when `a` is positive we return `b` (which may
    // itself be either sign). Net effect: if either operand is `-0`,
    // result is `-0`; otherwise `+0`. We collapse that into a single
    // factory call rather than re-dispatching through `roundToPrec`
    // (zero values have no prec-dependent payload to round).
    const wantNeg = a.sign === -1 || b.sign === -1;
    return {
      value: wantNeg ? negZero(prec) : posZero(prec),
      ternary: 0,
    };
  }

  // (4) General path: compareMPFR(a, b) decides. Neither operand is NaN
  // (the both-NaN and one-NaN cases were handled above), so the result
  // is in {-1, 0, +1} — null is impossible here.
  const cmp = compareMPFR(a, b);
  if (cmp === null) {
    // Defensive: unreachable given the NaN guards above. Surface a
    // precise error rather than mis-route.
    throw new MPFRError(
      'EPREC',
      `mpfr_min: compareMPFR returned null with non-NaN operands ` +
        `(a.kind=${a.kind}, b.kind=${b.kind})`,
    );
  }
  // mpfr/src/minmax.c L53–L56: `if (mpfr_cmp(x, y) <= 0) return
  // mpfr_set(z, x); else return mpfr_set(z, y);` — pick `a` on tie.
  return cmp <= 0 ? roundToPrec(a, prec, rnd) : roundToPrec(b, prec, rnd);
}
