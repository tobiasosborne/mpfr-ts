/**
 * ops/set4.ts — pure-TS port of MPFR's `mpfr_set4`.
 *
 * The load-bearing "set with explicit sign override" primitive. Every
 * sign-manipulating op in MPFR delegates here:
 *
 *   mpfr_set    → mpfr_set4(a, b, rnd, SIGN(b))
 *   mpfr_abs    → mpfr_set4(a, b, rnd, +1)
 *   mpfr_neg    → mpfr_set4(a, b, rnd, -SIGN(b))
 *   mpfr_setsign → mpfr_set4(a, b, rnd, s)
 *   mpfr_copysign → mpfr_set4(a, b, rnd, SIGN(c))
 *
 * C signature
 * -----------
 *
 *   int mpfr_set4(mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode, int signb)
 *
 *   - `a` is the output slot (precision comes from `a`), mutated in place;
 *   - `signb` is the REQUESTED OUTPUT SIGN, not necessarily b's sign;
 *   - returns ternary (sign of rounded - exact).
 *
 *   Ref: mpfr/src/set.c L25-L64 — the full C body.
 *
 * TS signature
 * ------------
 *
 *   mpfr_set4(b, prec, rnd, signb) -> Result
 *
 *   - `prec` is the explicit output precision in bits;
 *   - `signb` is the Sign (1 | -1) to impose on the result;
 *   - returns the immutable {value, ternary} shape from src/core.ts.
 *
 * Algorithm
 * ---------
 *
 * Mirrors mpfr/src/set.c L25-L64 step-by-step:
 *
 *   (1) NaN (b.kind === 'nan'): return canonical NAN_VALUE, ternary 0.
 *       The C MPFR_RET_NAN sets ERANGE; our schema collapses every NaN
 *       to NAN_VALUE with sign=1 (signb is discarded on NaN).
 *       Ref: mpfr/src/set.c L40-L43.
 *
 *   (2) Inf or Zero (b.kind === 'inf' | 'zero'): preserve kind, impose
 *       sign=signb, ternary 0.
 *       Ref: mpfr/src/set.c L43 (MPFR_RET(0) after EXP copy; the C sign
 *       was already set by MPFR_SET_SIGN(a, signb) at L29).
 *
 *   (3) Normal with prec === b.prec: lossless copy — mantissa identical,
 *       exponent identical, sign=signb, ternary 0.
 *       Ref: mpfr/src/set.c L45-L52 (MPFR_PREC(b) == MPFR_PREC(a) branch).
 *
 *   (4) Normal with prec !== b.prec: round via roundMantissa, passing
 *       `signb` (the OUTPUT sign) as the rounding-direction parameter,
 *       NOT b.sign. This is the critical invariant: C's MPFR_RNDRAW
 *       at L58 takes `signb` directly.
 *       Ref: mpfr/src/set.c L54-L63.
 *
 * Why signb (not b.sign) governs rounding direction
 * --------------------------------------------------
 *
 * Consider mpfr_set4(b, 3, RNDU, -1) where b = +1.1011 (5 bits).
 * The output has sign=-1, so |output| = -1.1011 rounded under RNDU.
 * Under RNDU (toward +∞), a negative output should be rounded toward
 * zero (magnitude shrinks). If we naively passed b.sign=+1 to
 * roundMantissa, RNDU would route as "increment" → magnitude grows →
 * wrong. Passing signb=-1 correctly routes RNDU as "truncate".
 * Same subtlety noted in src/ops/neg.ts "Why the rounding-sign
 * distinction matters".
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set.c L25-L64 — canonical C body.
 *   - src/internal/mpfr/round_raw.ts — substrate roundMantissa.
 *   - src/ops/neg.ts — signb = -SIGN(b); same rounding-sign note.
 *   - src/ops/abs.ts — signb = +1; same structure.
 *   - src/core.ts — locked MPFR / RoundingMode / Result / Sign types.
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real".
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is sign of
 *     (rounded - exact)" — at the OUTPUT sign (= signb).
 */

import type { MPFR, Result, RoundingMode, Sign } from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
} from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import { roundMantissa } from '/home/tobias/Projects/mpfr-ts/src/internal/mpfr/round_raw.ts';

/**
 * Validate the public-boundary scalar arguments.
 * Throws MPFRError on bad inputs.
 */
function validateArgs(prec: bigint, rnd: RoundingMode, signb: Sign): void {
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
  if (signb !== 1 && signb !== -1) {
    throw new MPFRError('EPREC', `signb must be 1 or -1, got ${String(signb)}`);
  }
}

/**
 * Copy b to a new MPFR value at the target precision, with an explicitly
 * provided output sign override. This is the load-bearing set primitive
 * that mpfr_set, mpfr_abs, mpfr_neg, mpfr_setsign, and mpfr_copysign
 * all delegate to.
 *
 * @mpfrName mpfr_set4
 *
 * @param b      the source value. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param prec   output precision in **bits**, in [PREC_MIN, PREC_MAX].
 * @param rnd    one of the five RoundingMode values.
 * @param signb  the OUTPUT sign to impose (1 for positive, -1 for negative).
 *               For NaN output, signb is discarded (NAN_VALUE.sign === 1).
 *               For Inf/Zero, signb determines the sign of the result.
 *               For normal, signb overrides b.sign AND governs rounding
 *               direction (per mpfr/src/set.c L58: MPFR_RNDRAW with signb).
 *
 * @returns {value, ternary}. Ternary is 0 for all specials and for
 *          same-precision or lossless-pad copies; ±1 when rounding loses bits.
 *
 * @throws {MPFRError} EPREC on bad precision or signb; EROUND on bad mode.
 *
 * Ref: mpfr/src/set.c L25-L64.
 */
export function mpfr_set4(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
  signb: Sign,
): Result {
  validateArgs(prec, rnd, signb);

  // --- (1) NaN ---------------------------------------------------------
  // Ref: mpfr/src/set.c L40-L43 — MPFR_IS_NAN(b) → MPFR_RET_NAN.
  // The TS schema collapses every NaN to NAN_VALUE (sign=1); signb is
  // discarded. Ternary 0.
  if (b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // --- (2) Inf / Zero --------------------------------------------------
  // Ref: mpfr/src/set.c L36-L44 — singular (not NaN) → copy EXP, MPFR_RET(0).
  // The C sign was already set by MPFR_SET_SIGN(a, signb) at L29.
  // We enforce sign=signb and ternary=0. Signed zero is observable.
  if (b.kind === 'inf') {
    return {
      value: signb === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  if (b.kind === 'zero') {
    return {
      value: signb === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // --- (3) & (4) Normal ------------------------------------------------
  // Defensive: unreachable given the four-way kind dispatch above.
  if (b.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_set4: unexpected kind ${String((b as { kind?: unknown }).kind)}`,
    );
  }

  // (3) Same precision: lossless copy. Mantissa, exponent unchanged.
  // Ref: mpfr/src/set.c L45-L52 — MPFR_PREC(b) == MPFR_PREC(a) branch.
  if (prec === b.prec) {
    const value: MPFR = {
      kind: 'normal',
      sign: signb,
      prec,
      exp: b.exp,
      mant: b.mant,
    };
    return { value, ternary: 0 };
  }

  // Prec differs from b.prec. Two sub-cases:
  if (prec > b.prec) {
    // Lossless pad: widen from b.prec to prec bits by left-shifting.
    // The numeric value is unchanged; only MSB-alignment frame changes.
    // Exact, ternary 0.
    const padShift = prec - b.prec;
    const value: MPFR = {
      kind: 'normal',
      sign: signb,
      prec,
      exp: b.exp,
      mant: b.mant << padShift,
    };
    return { value, ternary: 0 };
  }

  // (4) Lossy: round b.mant from b.prec bits down to prec bits.
  // Pass signb (the OUTPUT sign, not b.sign) to roundMantissa so that
  // RNDU/RNDD route correctly for the output's sign convention.
  // Ref: mpfr/src/set.c L54-L63 — MPFR_RNDRAW(inex, a, MPFR_MANT(b),
  //   MPFR_PREC(b), rnd_mode, signb, ...) — signb is the 6th argument.
  const { mant, exp, ternary } = roundMantissa(
    b.mant,
    b.prec,
    b.exp,
    prec,
    signb,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign: signb,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
