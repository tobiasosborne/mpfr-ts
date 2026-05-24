/**
 * reference_ports/broken/mpfr_max.ts — deliberately-buggy mpfr_max.
 *
 * **Deliberately broken: returns min(a, b) instead of max(a, b).** The dual
 * of broken/mpfr_min.ts — comparison branches and signed-zero polarity
 * swapped relative to the correct version.
 *
 * Behaviour:
 *   - Both NaN → NAN_VALUE (matches correct).
 *   - One NaN → other operand (matches correct).
 *   - Both zero → wrong polarity: returns -0 when either is negative,
 *     +0 otherwise (correct max: +0 if either is positive).
 *   - General: returns `a` on `a <= b`, `b` otherwise (min-style).
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/max.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negZero,
  posZero,
} from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';
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

function roundToPrec(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  if (x.kind === 'nan') return { value: NAN_VALUE, ternary: 0 };
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
  return { value: { kind: 'normal', sign: x.sign, prec, exp, mant }, ternary };
}

export function mpfr_max(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);
  const aNan = a.kind === 'nan';
  const bNan = b.kind === 'nan';
  if (aNan && bNan) return { value: NAN_VALUE, ternary: 0 };
  if (aNan) return roundToPrec(b, prec, rnd);
  if (bNan) return roundToPrec(a, prec, rnd);

  // BUG: should pick positive polarity. Picks negative (min-style).
  if (a.kind === 'zero' && b.kind === 'zero') {
    const wantNeg = a.sign === -1 || b.sign === -1;
    return {
      value: wantNeg ? negZero(prec) : posZero(prec),
      ternary: 0,
    };
  }

  const cmp = compareMPFR(a, b);
  if (cmp === null) {
    throw new MPFRError('EPREC', `mpfr_max(broken): unexpected null cmp`);
  }
  // BUG: should be `cmp <= 0 ? b : a`. Returns the OPPOSITE — i.e. min.
  return cmp <= 0 ? roundToPrec(a, prec, rnd) : roundToPrec(b, prec, rnd);
}
