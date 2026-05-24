/**
 * reference_ports/broken/mpfr_mul_2si.ts — deliberately-buggy mpfr_mul_2si.
 *
 * **Deliberately broken: ignores `e`** — returns x at the target prec
 * without shifting the exponent. Every case where `e !== 0n` fails on
 * the exp field (and on the value as a result); e=0 cases still pass,
 * but the golden distributes e across non-zero values heavily enough
 * that composite ≤ 0.5.
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: composite ≤ 0.5.
 *
 * Ref: src/ops/mul_2si.ts — the correct version.
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
  validate,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

function validateArgs(e: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof e !== 'bigint') {
    throw new MPFRError('EPREC', `e must be bigint, got ${typeof e}`);
  }
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

export function mpfr_mul_2si(
  x: MPFR,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(e, prec, rnd);
  validate(x);

  // BUG: ignores `e` entirely.
  void e;

  switch (x.kind) {
    case 'nan':
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      return { value: x.sign === 1 ? posInf(prec) : negInf(prec), ternary: 0 };
    case 'zero':
      return {
        value: x.sign === 1 ? posZero(prec) : negZero(prec),
        ternary: 0,
      };
    case 'normal':
      break;
  }

  let postExp: bigint;
  let postMant: bigint;
  let ternary: -1 | 0 | 1;

  if (prec >= x.prec) {
    postMant = x.mant << (prec - x.prec);
    postExp = x.exp;
    ternary = 0;
  } else {
    const { mant, exp, ternary: tr } = roundMantissa(
      x.mant,
      x.prec,
      x.exp,
      prec,
      x.sign,
      rnd,
    );
    postMant = mant;
    postExp = exp;
    ternary = tr;
  }

  const value: MPFR = {
    kind: 'normal',
    sign: x.sign,
    prec,
    exp: postExp, // BUG: should be postExp + e
    mant: postMant,
  };
  return { value, ternary };
}
