/**
 * reference_ports/broken/mpfr_setsign.ts — deliberately-buggy mpfr_setsign.
 *
 * **Deliberately broken: ignores the requested sign and returns x unchanged
 * (modulo prec refit).** Every non-NaN branch preserves x.sign instead of
 * using newSign. This mirrors a plausible agent error: "I copied mpfr_set,
 * not mpfr_setsign — forgot to apply the sign override."
 *
 * Behaviour:
 *   - NaN → NAN_VALUE (matches correct).
 *   - ±Inf → same-sign Inf (bug: should follow `sign` argument).
 *   - ±0 → same-sign zero (bug: should follow `sign`).
 *   - normal → x rounded to prec with the ORIGINAL sign (bug: should
 *     follow `sign`; ternary is computed against x's sign which happens
 *     to be self-consistent — so the failure surfaces as a sign
 *     mismatch on cases where `sign != (x.sign === -1)`).
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/setsign.ts — the correct version.
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

export function mpfr_setsign(
  x: MPFR,
  sign: boolean,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // Suppress unused-parameter complaint without using the value. The bug
  // is the deliberate non-use of `sign`.
  void sign;

  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // BUG: should use the requested sign. Preserves x.sign.
  if (x.kind === 'inf') {
    return {
      value: x.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // BUG: same.
  if (x.kind === 'zero') {
    return {
      value: x.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_setsign(broken): unexpected kind ${String(x.kind)}`,
    );
  }

  // BUG: should be newSign = sign ? -1 : 1. Uses x.sign.
  const useSign = x.sign;

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
