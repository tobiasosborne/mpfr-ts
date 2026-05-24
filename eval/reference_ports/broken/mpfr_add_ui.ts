/**
 * reference_ports/broken/mpfr_add_ui.ts — deliberately-buggy mpfr_add_ui.
 *
 * **Multi-bug perturbation:**
 *   1. For normal b: delegates to mpfr_sub(b, cMPFR, prec, rnd) instead
 *      of mpfr_add. Every normal case fails on value.
 *   2. For c == 0: returns mpfr_set(b, prec, RNDD) instead of the
 *      caller's rnd. Some cases fail on rounding.
 *   3. For NaN b: returns posZero(prec) instead of NAN_VALUE.
 *
 * NOT used in production.
 *
 * Ref: src/ops/add_ui.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  negInf,
  posInf,
  posZero,
} from '../../../src/core.ts';
import { mpfr_set } from '../../../src/ops/set.ts';
import { mpfr_set_ui } from '../../../src/ops/set_ui.ts';
import { mpfr_sub } from '../../../src/ops/sub.ts';

const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

function bitLength(n: bigint): bigint {
  if (n === 0n) return 0n;
  let bits = 0n;
  let probe = n;
  while (probe > 0n) { bits++; probe >>= 1n; }
  return bits;
}

export function mpfr_add_ui(
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

  if (c === 0n) {
    // BUG 2: ignore caller's rnd.
    return mpfr_set(b, prec, 'RNDD');
  }
  switch (b.kind) {
    case 'nan':
      // BUG 3: return posZero instead of NaN.
      return { value: posZero(prec), ternary: 0 };
    case 'inf':
      return {
        value: b.sign === 1 ? posInf(prec) : negInf(prec),
        ternary: 0,
      };
    case 'zero':
      return mpfr_set_ui(c, prec, rnd);
    case 'normal':
      break;
  }
  const cPrec = bitLength(c);
  const cMPFR = mpfr_set_ui(c, cPrec, 'RNDN').value;
  // BUG 1: mpfr_sub instead of mpfr_add.
  return mpfr_sub(b, cMPFR, prec, rnd);
}
