/**
 * reference_ports/broken/mpfr_trunc.ts — deliberately-buggy mpfr_trunc.
 *
 * **Deliberately broken: rounds AWAY from zero (RNDA / ceil-of-magnitude)
 * instead of truncating toward zero.** Whenever x has a non-zero
 * fractional part, the broken port increments the truncated magnitude
 * by 1. Mirrors a plausible agent error: "I confused RNDZ and RNDA in
 * the mpfr_rint rnd_away decision."
 *
 * Behaviour:
 *   - NaN/Inf/Zero: matches correct.
 *   - 0.3 → 1 (correct says +0).
 *   - -0.3 → -1 (correct says -0).
 *   - 2.3 → 3 (correct says 2).
 *   - -2.3 → -3 (correct says -2).
 *   - 2.0 → 2 (matches correct — already integer).
 *
 * The behavioural difference from correct is "any non-integer x", which
 * is most cases — the gap-from-correct on the golden should be large.
 *
 * Ref: src/ops/trunc.ts — the correct version.
 */

import type { MPFR, Result, Sign } from '../../../src/core.ts';
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

function validatePrec(prec: bigint): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
}

// BROKEN: implements RNDA (round-away-from-zero, ceil-of-magnitude)
// instead of RNDZ.
export function mpfr_trunc(x: MPFR, prec: bigint): Result {
  validatePrec(prec);

  if (x.kind === 'nan') return { value: NAN_VALUE, ternary: 0 };
  if (x.kind === 'inf') {
    return {
      value: x.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }
  if (x.kind === 'zero') {
    return {
      value: x.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }
  if (x.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_trunc(broken): unexpected kind`);
  }

  // BUG: |x| < 1 → result is sign*1 instead of sign*0.
  if (x.exp <= 0n) {
    const value: MPFR = {
      kind: 'normal',
      sign: x.sign,
      prec,
      exp: 1n,
      mant: 1n << (prec - 1n),
    };
    return { value, ternary: x.sign === 1 ? 1 : -1 };
  }

  // |x| >= 1: compute the truncated mantissa, then ALWAYS increment
  // magnitude when any bit was dropped (RNDA semantics).
  const xExp = x.exp;
  const xPrec = x.prec;
  const xMant = x.mant;
  const sign: Sign = x.sign;

  let truncMant: bigint;
  let droppedAny: boolean;
  if (xExp >= prec) {
    if (xPrec >= prec) {
      const shift = xPrec - prec;
      truncMant = xMant >> shift;
      droppedAny = shift > 0n && (xMant & ((1n << shift) - 1n)) !== 0n;
    } else {
      truncMant = xMant << (prec - xPrec);
      droppedAny = false;
    }
  } else {
    if (xPrec > xExp) {
      const fracBitsCount = xPrec - xExp;
      const intAbs = xMant >> fracBitsCount;
      droppedAny = (xMant & ((1n << fracBitsCount) - 1n)) !== 0n;
      truncMant = intAbs << (prec - xExp);
    } else {
      truncMant = xMant << (prec - xPrec);
      droppedAny = false;
    }
  }

  if (!droppedAny) {
    const value: MPFR = { kind: 'normal', sign, prec, exp: xExp, mant: truncMant };
    return { value, ternary: 0 };
  }

  // BUG: always increment magnitude (RNDA).
  const ulp = xExp >= prec ? 1n : 1n << (prec - xExp);
  const incremented = truncMant + ulp;
  const upperBound = 1n << prec;
  let mant: bigint;
  let exp: bigint;
  if (incremented === upperBound) {
    mant = upperBound >> 1n;
    exp = xExp + 1n;
  } else {
    mant = incremented;
    exp = xExp;
  }
  const value: MPFR = { kind: 'normal', sign, prec, exp, mant };
  return { value, ternary: sign === 1 ? 1 : -1 };
}
