/**
 * reference_ports/broken/mpfr_mul_ui.ts — deliberately-buggy mpfr_mul_ui.
 *
 * **Multi-bug perturbation:**
 *   1. ±Inf * 0: returns ±Inf (sign preserved) instead of NaN.
 *      Indeterminate-form bug, very common naive mistake.
 *   2. ±0 * c: drops sign (always returns +0).
 *   3. General path: delegates to mpfr_div instead of mpfr_mul.
 *
 * NOT used in production.
 *
 * Ref: src/ops/mul_ui.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  posInf,
  posZero,
} from '../../../src/core.ts';
import { mpfr_div } from '../../../src/ops/div.ts';
import { mpfr_set } from '../../../src/ops/set.ts';
import { mpfr_set_ui } from '../../../src/ops/set_ui.ts';

const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

function bitLength(n: bigint): bigint {
  if (n === 0n) return 0n;
  let bits = 0n;
  let probe = n;
  while (probe > 0n) { bits++; probe >>= 1n; }
  return bits;
}

export function mpfr_mul_ui(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  if (typeof c !== 'bigint') throw new MPFRError('EPREC', `c bigint`);
  if (c < 0n || c > ULONG_MAX_VAL) throw new MPFRError('EPREC', `c range`);
  if (typeof prec !== 'bigint') throw new MPFRError('EPREC', `prec bigint`);
  if (prec < PREC_MIN || prec > PREC_MAX) throw new MPFRError('EPREC', `prec`);
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `rnd`);
  }

  switch (b.kind) {
    case 'nan':
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      // BUG 1: should be NaN for c == 0.
      return {
        value: b.sign === 1 ? posInf(prec) : negInf(prec),
        ternary: 0,
      };
    case 'zero':
      // BUG 2: drop sign.
      return { value: posZero(prec), ternary: 0 };
    case 'normal':
      break;
  }

  if (c === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }
  if (c === 1n) {
    return mpfr_set(b, prec, rnd);
  }

  const cPrec = bitLength(c);
  const cMPFR = mpfr_set_ui(c, cPrec, 'RNDN').value;
  // BUG 3: mpfr_div instead of mpfr_mul.
  return mpfr_div(b, cMPFR, prec, rnd);
}
