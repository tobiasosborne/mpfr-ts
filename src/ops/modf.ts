/**
 * reference_ports/correct/mpfr_modf.ts -- mutation-prove reference.
 *
 * Splits an MPFR value op into its integer part (trunc) and fractional
 * part (frac), each returned with its own ternary. The sign of op
 * propagates to both parts; for NaN both parts are NaN; for +/-Inf the
 * integer part is +/-Inf and the fractional part is +/-0 (same sign);
 * for +/-0 both parts are +/-0 (same sign).
 *
 * Algorithm (mpfr/src/modf.c L24-L98):
 *
 *   1. NaN     -> {iop: NAN, fop: NAN}, both ternary 0.
 *   2. +/-Inf  -> {iop: +/-Inf, fop: +/-0}, both ternary 0.
 *   3. +/-0    -> {iop: +/-0, fop: +/-0}, both ternary 0.
 *   4. exp <= 0 (|op| < 1):
 *        iop = posZero or negZero per op.sign
 *        fop = mpfr_set(op, fprec, rnd)
 *   5. exp >= op.prec (op has no fractional bits, even after rounding):
 *        iop = mpfr_set(op, iprec, rnd)
 *        fop = posZero or negZero per op.sign
 *   6. Otherwise: iop = mpfr_rint_trunc(op, iprec, rnd);
 *                 fop = mpfr_frac(op, fprec, rnd).
 *
 * Delegates to already-shipped:
 *   - src/ops/set.ts        (mpfr_set, status=done)
 *   - src/ops/rint_trunc.ts (mpfr_rint_trunc, status=done)
 *   - src/ops/frac.ts       (mpfr_frac, status=done)
 *
 * Ref: mpfr/src/modf.c L24-L98 -- C reference.
 * Ref: mpfr/src/mpfr-impl.h L1200-L1201 -- INEX macro (the driver
 *   unpacks; the TS port returns separate Results directly).
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
import { mpfr_set } from './set.ts';
import { mpfr_rint_trunc } from './rint_trunc.ts';
import { mpfr_frac } from './frac.ts';

function assertPrec(name: string, prec: bigint): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_modf: ${name} must be bigint`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `mpfr_modf: ${name} must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_modf: ${name} must be <= ${PREC_MAX}, got ${prec}`);
  }
}

function assertRnd(rnd: RoundingMode): void {
  if (
    rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `mpfr_modf: unknown rounding mode: ${String(rnd)}`);
  }
}

function zeroOfSign(sign: 1 | -1, prec: bigint): MPFR {
  return sign === 1 ? posZero(prec) : negZero(prec);
}

export function mpfr_modf(
  x: MPFR,
  iprec: bigint,
  fprec: bigint,
  rnd: RoundingMode,
): { iop: Result; fop: Result } {
  assertPrec('iprec', iprec);
  assertPrec('fprec', fprec);
  assertRnd(rnd);

  // (1) NaN: both parts NaN.
  if (x.kind === 'nan') {
    return {
      iop: { value: NAN_VALUE, ternary: 0 },
      fop: { value: NAN_VALUE, ternary: 0 },
    };
  }

  // (2) +/-Inf: iop = +/-Inf at iprec; fop = +/-0 at fprec.
  if (x.kind === 'inf') {
    const sign = x.sign;
    const iopInf: MPFR = { kind: 'inf', sign, prec: iprec, exp: 0n, mant: 0n };
    return {
      iop: { value: iopInf, ternary: 0 },
      fop: { value: zeroOfSign(sign, fprec), ternary: 0 },
    };
  }

  // (3) +/-0: both parts +/-0 at requested precs.
  if (x.kind === 'zero') {
    return {
      iop: { value: zeroOfSign(x.sign, iprec), ternary: 0 },
      fop: { value: zeroOfSign(x.sign, fprec), ternary: 0 },
    };
  }

  // x is normal here. Schema: |x| in [2^(exp-1), 2^exp).
  // C reference uses `ope = MPFR_GET_EXP(op)` (the same convention).
  const ope = x.exp;
  const opq = x.prec;

  // (4) ope <= 0: |x| < 1. iop is +/-0; fop = mpfr_set(x, fprec, rnd).
  if (ope <= 0n) {
    const fopResult = mpfr_set(x, fprec, rnd);
    return {
      iop: { value: zeroOfSign(x.sign, iprec), ternary: 0 },
      fop: fopResult,
    };
  }

  // (5) ope >= opq: x has no fractional part.
  // iop = mpfr_set(x, iprec, rnd); fop = +/-0.
  if (ope >= opq) {
    const iopResult = mpfr_set(x, iprec, rnd);
    return {
      iop: iopResult,
      fop: { value: zeroOfSign(x.sign, fprec), ternary: 0 },
    };
  }

  // (6) Mixed: iop = rint_trunc(x, iprec, rnd); fop = frac(x, fprec, rnd).
  // The C reference handles aliasing (iop===op or fop===op); the immutable
  // TS surface has no aliasing so the order of evaluation doesn't matter.
  const iopResult = mpfr_rint_trunc(x, iprec, rnd);
  const fopResult = mpfr_frac(x, fprec, rnd);
  return { iop: iopResult, fop: fopResult };
}
