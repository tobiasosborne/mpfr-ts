/**
 * reference_ports/broken/mpfr_sub_ui.ts — deliberately-buggy mpfr_sub_ui.
 *
 * **Multi-bug perturbation:**
 *   1. Delegates to mpfr_add instead of mpfr_sub.
 *   2. For NaN b: returns posZero(prec) instead of NAN_VALUE.
 *   3. For ±Inf b: drops sign (always returns +Inf).
 *
 * NOT used in production.
 *
 * Ref: src/ops/sub_ui.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posInf,
  posZero,
} from '../../../src/core.ts';
import { mpfr_add } from '../../../src/ops/add.ts';
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

export function mpfr_sub_ui(
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

  if (c === 0n) return mpfr_set(b, prec, rnd);

  switch (b.kind) {
    case 'nan':
      // BUG 2: return posZero instead of NAN.
      return { value: posZero(prec), ternary: 0 };
    case 'inf':
      // BUG 3: drop sign.
      return { value: posInf(prec), ternary: 0 };
    case 'zero':
    case 'normal':
      break;
  }
  const cPrec = bitLength(c);
  const cMPFR = mpfr_set_ui(c, cPrec, 'RNDN').value;
  // BUG 1: mpfr_add instead of mpfr_sub.
  return mpfr_add(b, cMPFR, prec, rnd);
}
