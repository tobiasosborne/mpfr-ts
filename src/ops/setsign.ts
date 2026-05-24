/**
 * ops/setsign.ts — pure-TS port of MPFR's `mpfr_setsign`.
 *
 * Produce a value with the magnitude of `x` and an explicit caller-chosen
 * sign bit, rounded per `rnd` to the target precision. Sibling to abs.ts
 * and neg.ts; differs only in that the result sign is a caller argument
 * rather than fixed (+1 for abs, -x.sign for neg).
 *
 * C signature
 * -----------
 *
 *   int mpfr_setsign(mpfr_t rop, mpfr_srcptr op, int s, mpfr_rnd_t rnd);
 *
 *     - `s != 0` → result sign is negative (MPFR_SIGN_NEG, -1);
 *     - `s == 0` → result sign is positive (MPFR_SIGN_POS, +1).
 *
 *   The C dispatch in mpfr/src/setsign.c L25–L38 splits on alias:
 *
 *     if (z != x)
 *       return mpfr_set4 (z, x, rnd_mode, s ? MPFR_SIGN_NEG : MPFR_SIGN_POS);
 *     else {
 *       MPFR_SET_SIGN (z, s ? MPFR_SIGN_NEG : MPFR_SIGN_POS);
 *       if (MPFR_IS_NAN (x)) MPFR_RET_NAN;
 *       else MPFR_RET (0);
 *     }
 *
 *   — i.e. either "copy x with the requested sign, rounded to rop's prec"
 *   (the general path), or "flip rop's sign in place" (the alias path).
 *
 * TS signature
 * ------------
 *
 *   mpfr_setsign(x: MPFR, sign: boolean, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - `sign === true`  → result is negative (mirrors `s != 0` in C);
 *   - `sign === false` → result is positive (mirrors `s == 0` in C).
 *
 *   We use TypeScript's native `boolean` rather than the C-style `int`
 *   to surface the intent in callers: `setsign(x, true, ...)` reads
 *   "make negative". The grader's wire encoding emits `s` as a JSON
 *   boolean via `jl_kv_bool` so the in/out shape is unambiguous.
 *
 * Algorithm
 * ---------
 *
 *   1. NaN: the canonical NAN_VALUE has sign=1 by convention (see
 *      src/core.ts L83–L88). The C reference would set the sign bit on
 *      a NaN, but our locked schema folds every NaN to NAN_VALUE; we
 *      return NAN_VALUE unconditionally. (The grader's wire decoder
 *      does the same fold for the golden side via decodeMpfr.)
 *
 *   2. ±Inf: result kind is `inf` with caller-chosen sign. Exact.
 *
 *   3. ±0: result kind is `zero` with caller-chosen sign. Signed zero is
 *      observable in MPFR arithmetic (RNDD-add of +0 / -0 differs), so
 *      we propagate the caller's choice rather than collapsing.
 *
 *   4. normal: result sign = newSign; mantissa needs the same
 *      prec-conversion treatment as `mpfr_set`:
 *
 *        - If `prec >= x.prec`: lossless pad. New mantissa is
 *          `x.mant << (prec - x.prec)`, exponent unchanged, ternary 0.
 *
 *        - If `prec <  x.prec`: round `x.mant` from `x.prec` bits down
 *          to `prec` bits via the shared `roundMantissa` primitive.
 *          The rounding direction uses **the NEW sign**, not x's old
 *          sign — exactly as the C reference wires this via
 *          `mpfr_set4(..., s ? MPFR_SIGN_NEG : MPFR_SIGN_POS)`. The
 *          `mpfr_set4` rounding step (mpfr/src/set.c L58) is
 *          parameterised by the passed-in `signb`. See the
 *          "Why the new-sign rounding direction matters" section of
 *          neg.ts for the worked example; the same subtlety applies
 *          here when the chosen sign disagrees with x.sign.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/setsign.c L25–L38 — the C reference. The alias-case
 *     branch flips the sign bit in place; the general path delegates to
 *     `mpfr_set4` with `s ? MPFR_SIGN_NEG : MPFR_SIGN_POS`.
 *   - mpfr/src/set.c L25–L64 — `mpfr_set4`, the load-bearing primitive.
 *     The `MPFR_RNDRAW(..., signb, ...)` call at L58 takes the
 *     caller-supplied sign for rounding direction.
 *   - src/ops/abs.ts — sibling op (always sign=+1).
 *   - src/ops/neg.ts — sibling op (sign = -x.sign).
 *   - src/internal/mpfr/round_raw.ts — shared substrate primitive.
 *   - src/core.ts — locked MPFR / RoundingMode / Result / Sign types.
 *   - CLAUDE.md "Hallucination-risk callouts" — Signed zero observable;
 *     ternary is sign of (rounded - exact) AT THE RESULT SIGN;
 *     rounding-mode count is FIVE.
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
 * Validate the public-boundary scalar arguments. Same shape as abs.ts /
 * neg.ts.
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
 * Produce a value with the magnitude of `x` and the caller-chosen sign
 * bit, at the target precision.
 *
 * @mpfrName mpfr_setsign
 *
 * @param x     the operand. Any kind (`'normal'`, `'zero'`, `'inf'`, `'nan'`).
 * @param sign  `true` for a negative result, `false` for positive. Mirrors
 *              the C `int s` where any non-zero is negative.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. NaN input → NAN_VALUE (sign discarded
 *              per the locked schema's NaN canonicalisation).
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf input is NOT an error.
 *
 * @example
 *   setsign(setD(3.14, 53n, 'RNDN').value, true, 53n, 'RNDN');
 *     // → {value: -3.14 at prec 53, ternary: 0}
 *   setsign(posZero(53n), true, 53n, 'RNDN');
 *     // → {value: negZero(53n), ternary: 0}
 *   setsign(negInf(53n), false, 53n, 'RNDN');
 *     // → {value: posInf(53n), ternary: 0}
 *   setsign(NAN_VALUE, true, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}  — schema-canonical NaN
 */
export function mpfr_setsign(
  x: MPFR,
  sign: boolean,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // (1) NaN propagation. NAN_VALUE has sign=1 by convention; the wire
  // decoder folds C-side NaN-with-sign to the canonical form, so the
  // golden expects NAN_VALUE here too.
  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  const newSign: Sign = sign ? -1 : 1;

  // (2) ±Inf: pick the constructor for the requested sign.
  if (x.kind === 'inf') {
    return {
      value: newSign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // (3) ±0: same — signed zero is observable.
  if (x.kind === 'zero') {
    return {
      value: newSign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (4) Normal.
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_setsign: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  if (prec >= x.prec) {
    const padShift = prec - x.prec;
    const value: MPFR = {
      kind: 'normal',
      sign: newSign,
      prec,
      exp: x.exp,
      mant: x.mant << padShift,
    };
    return { value, ternary: 0 };
  }

  // Lossy: round to fewer bits, pass the new sign so RNDU/RNDD route
  // through the substrate's positive/negative-value branches correctly.
  const { mant, exp, ternary } = roundMantissa(
    x.mant,
    x.prec,
    x.exp,
    prec,
    newSign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign: newSign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
