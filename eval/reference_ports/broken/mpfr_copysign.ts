/**
 * reference_ports/broken/mpfr_copysign.ts — deliberately-buggy mpfr_copysign.
 *
 * **Deliberately broken: copies x's sign instead of y's.** Returns x with
 * x's original sign (modulo prec refit), ignoring y entirely. Mirrors a
 * plausible agent error: "I confused the source operand for copysign —
 * thought x carried the sign."
 *
 * Behaviour:
 *   - NaN x → NAN_VALUE (matches correct, since both fold to sign=1).
 *   - ±Inf x → ±Inf with x's sign (bug: should be y's sign).
 *   - ±0 x → ±0 with x's sign (bug: should be y's).
 *   - normal x → x rounded to prec with x's sign (bug).
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/copysign.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

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

export function mpfr_copysign(
  x: MPFR,
  y: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // BUG: y is ignored entirely below; this `void y` is here to keep the
  // unused-parameter check from rejecting the file. The bug is the
  // deliberate non-use of y's sign.
  void y;

  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // BUG: should use y.sign. Uses x.sign.
  const useSign = x.sign;

  if (x.kind === 'inf') {
    return {
      value: useSign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }
  if (x.kind === 'zero') {
    return {
      value: useSign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_copysign(broken): unexpected kind ${String(x.kind)}`,
    );
  }

  if (prec >= x.prec) {
    const padShift = prec - x.prec;
    return {
      value: {
        kind: 'normal',
        sign: useSign,
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
    useSign,
    rnd,
  );
  return {
    value: { kind: 'normal', sign: useSign, prec, exp, mant },
    ternary,
  };
}
