/**
 * reference_ports/broken/mpfr_div_2ui.ts — deliberately-buggy mpfr_div_2ui.
 *
 * **Multi-bug perturbation:**
 *   1. For kind='normal': uses `postExp + n` instead of `postExp - n`
 *      (multiply instead of divide).
 *   2. For zero: returns +0 unconditionally (drops sign).
 *   3. For inf: returns +inf unconditionally (drops sign).
 *
 * NOT used in production.
 *
 * Ref: src/ops/div_2ui.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  posInf,
  posZero,
  validate,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

function validateArgs(n: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof n !== 'bigint') throw new MPFRError('EPREC', `n must be bigint`);
  if (n < 0n) throw new MPFRError('EPREC', `n must be >= 0`);
  if (typeof prec !== 'bigint') throw new MPFRError('EPREC', `prec must be bigint`);
  if (prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec out of range`);
  }
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

export function mpfr_div_2ui(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(n, prec, rnd);
  validate(x);

  switch (x.kind) {
    case 'nan':
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      return { value: posInf(prec), ternary: 0 };
    case 'zero':
      return { value: posZero(prec), ternary: 0 };
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
      x.mant, x.prec, x.exp, prec, x.sign, rnd,
    );
    postMant = mant;
    postExp = exp;
    ternary = tr;
  }

  return {
    // BUG 1: + n instead of - n.
    value: { kind: 'normal', sign: x.sign, prec, exp: postExp + n, mant: postMant },
    ternary,
  };
}
