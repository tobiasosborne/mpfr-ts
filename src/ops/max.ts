/**
 * ops/max.ts — pure-TS port of MPFR's `mpfr_max`.
 *
 * Return the larger of two {@link MPFR} operands, rounded to the target
 * precision per the rounding mode. Sibling to min.ts; structurally
 * identical with the comparison-direction and signed-zero branches
 * flipped. The shared "round to prec preserving sign" helper is
 * duplicated here (rather than imported across siblings) because the
 * substrate primitive belongs to the operand and not to the op pair —
 * and because the two-file copy is six lines, cheaper than a new
 * substrate module.
 *
 * C signature
 * -----------
 *
 *   int mpfr_max(mpfr_t rop, mpfr_srcptr x, mpfr_srcptr y, mpfr_rnd_t rnd);
 *
 *   Returns the ternary. Sets `rop` to `max(x, y)` per IEEE 754-2008
 *   `maxNum` semantics: NaN ignored if the other operand is non-NaN;
 *   both NaN → NaN. Signed-zero ordering is `+0 > -0` (the dual of
 *   min). Ref: mpfr/src/minmax.c L65–L91.
 *
 *   The signed-zero pre-test (mpfr/src/minmax.c L79–L85) mirrors min's:
 *   when both operands are zero the result is `+0` if either is
 *   positive, else `-0`. This guards against `mpfr_cmp`'s `0` return
 *   on the both-zero pair which would otherwise mis-route.
 *
 * TS signature
 * ------------
 *
 *   mpfr_max(a: MPFR, b: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 *   1. Both NaN → NAN_VALUE.
 *   2. One NaN → the non-NaN operand, rounded to prec.
 *   3. Both zero → `+0` if either is positive, else `-0`.
 *   4. Otherwise: dispatch on `compareMPFR(a, b)`:
 *        - `<= 0` → return `b`, rounded to prec.
 *        - `>  0` → return `a`, rounded to prec.
 *      (Mirrors mpfr/src/minmax.c L87–L90: `if (mpfr_cmp(x, y) <= 0)
 *      return mpfr_set(z, y); else return mpfr_set(z, x);` — `b` on
 *      tie, the opposite of `min`'s `a` on tie.)
 *
 * Refs
 * ----
 *
 *   - mpfr/src/minmax.c L65–L91 — the C reference.
 *   - mpfr/src/cmp.c L32–L98 — `mpfr_cmp3`.
 *   - src/internal/mpfr/cmp_raw.ts — substrate compare.
 *   - src/ops/min.ts — direct sibling; same structure with comparison
 *     and signed-zero branches reversed.
 *   - src/internal/mpfr/round_raw.ts — substrate rounding primitive.
 *   - src/core.ts — locked types and factories.
 *   - CLAUDE.md "Hallucination-risk callouts" — Signed zero observable
 *     (max(+0, -0) = +0); NaN ≠ NaN (IEEE 754-2008 maxNum); rounding-
 *     mode count is FIVE.
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
 * Refit `x` to the target precision, preserving kind and sign. See
 * src/ops/min.ts for the shared rationale; duplicated here so the two
 * ops can be inspected independently.
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
 * Return `max(a, b)` rounded to the target precision.
 *
 * Per IEEE 754-2008 `maxNum`: when one operand is NaN the other is
 * returned; both NaN → NaN; signed zeros order as `+0 > -0`.
 *
 * @mpfrName mpfr_max
 *
 * @param a     left operand. Any kind.
 * @param b     right operand. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}` — the larger operand rounded to prec.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf inputs are NOT errors.
 *
 * @example
 *   max(setD(1, 53n, 'RNDN').value, setD(2, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → 2, ternary 0
 *   max(NAN_VALUE, setD(0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → 0, ternary 0
 *   max(negZero(53n), posZero(53n), 53n, 'RNDN');
 *     // → +0, ternary 0
 */
export function mpfr_max(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  const aNan = a.kind === 'nan';
  const bNan = b.kind === 'nan';

  // (1) Both NaN.
  if (aNan && bNan) {
    return { value: NAN_VALUE, ternary: 0 };
  }
  // (2) Exactly one NaN — return the other.
  if (aNan) return roundToPrec(b, prec, rnd);
  if (bNan) return roundToPrec(a, prec, rnd);

  // (3) Both zero — return `+0` if either is positive, else `-0`. The
  // C source (mpfr/src/minmax.c L79–L85) returns the positive operand
  // when one is positive: `if (MPFR_IS_NEG(x)) return mpfr_set(z, y);
  // else return mpfr_set(z, x);`. So if `a` is negative we return `b`
  // (which may be either sign); if `a` is positive we return `a`.
  // Reducing both arms: result is positive iff either operand is
  // positive.
  if (a.kind === 'zero' && b.kind === 'zero') {
    const wantPos = a.sign === 1 || b.sign === 1;
    return {
      value: wantPos ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (4) General path. Per mpfr/src/minmax.c L87–L90:
  //   if (mpfr_cmp(x, y) <= 0) return mpfr_set(z, y);
  //   else                    return mpfr_set(z, x);
  // — i.e. on `a <= b` return `b`; otherwise return `a`.
  const cmp = compareMPFR(a, b);
  if (cmp === null) {
    throw new MPFRError(
      'EPREC',
      `mpfr_max: compareMPFR returned null with non-NaN operands ` +
        `(a.kind=${a.kind}, b.kind=${b.kind})`,
    );
  }
  return cmp <= 0 ? roundToPrec(b, prec, rnd) : roundToPrec(a, prec, rnd);
}
