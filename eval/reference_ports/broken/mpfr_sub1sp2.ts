/**
 * reference_ports/broken/mpfr_sub1sp2.ts — deliberately-buggy mpfr_sub1sp2.
 *
 * **Deliberately broken: use the WRONG sign for rounding direction** —
 * pass `b.sign` instead of the post-comparison `sign` (which may be
 * `-b.sign` when |c| > |b|). Effect: every case where |c| > |b| and
 * RNDU/RNDD is asymmetric will round in the wrong direction with the
 * wrong ternary.
 *
 * Also: skip the cancellation-zero RNDD sign rule (always +0).
 *
 * Composite drops below 0.55.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { MPFRError, posZero } from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';
import { mpfr_underflow } from '../../../src/ops/underflow.ts';

const EMAX_DEFAULT = (1n << 30n) - 1n;
const EMIN_DEFAULT = -((1n << 30n) - 1n);

function bitLen(x: bigint): bigint {
  if (x <= 0n) return 0n;
  return BigInt(x.toString(2).length);
}

function cmpMag(b: MPFR, c: MPFR): -1 | 0 | 1 {
  if (b.exp !== c.exp) return b.exp < c.exp ? -1 : 1;
  if (b.mant === c.mant) return 0;
  return b.mant < c.mant ? -1 : 1;
}

export function mpfr_sub1sp2(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'sub1sp2(broken): non-normal');
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', 'sub1sp2(broken): prec mismatch');
  }
  if (b.prec <= 64n || b.prec >= 128n) {
    throw new MPFRError('EPREC', 'sub1sp2(broken): prec out of range');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'sub1sp2(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'sub1sp2(broken): bad rnd');
  }

  const p = b.prec;
  const cmp = cmpMag(b, c);
  if (cmp === 0) {
    // BUG: always return +0 regardless of rnd_mode.
    return { value: posZero(p), ternary: 0 };
  }

  let large: MPFR, small: MPFR;
  let sign: Sign;
  if (cmp === 1) {
    large = b; small = c; sign = b.sign;
  } else {
    large = c; small = b; sign = -b.sign as Sign;
  }

  const d = large.exp - small.exp;
  // BUG: forget to shift large.mant by d when aligning. This corrupts the
  // result whenever the operands have different exponents.
  const D = large.mant - small.mant;
  if (d > 0n) {
    // (the genuine D would be larger; we use this corrupted form to
    // produce a wrong but valid-shaped output for grading)
    // Fall through with the buggy D.
  }

  if (D === 0n) {
    return { value: posZero(p), ternary: 0 };
  }

  const bl = bitLen(D);
  const bxIn = bl + (small.exp - p);

  let mant: bigint;
  let exp: bigint;
  let ternary: Ternary;
  if (bl <= p) {
    mant = D << (p - bl);
    exp = bxIn;
    ternary = 0;
  } else {
    // BUG: pass b.sign instead of the post-comparison sign.
    const rounded = roundMantissa(D, bl, bxIn, p, b.sign, rnd);
    mant = rounded.mant;
    exp = rounded.exp;
    ternary = rounded.ternary;
  }

  if (exp < EMIN_DEFAULT) {
    return mpfr_underflow(p, rnd, sign);
  }

  if (exp > EMAX_DEFAULT) {
    throw new MPFRError('EPREC', 'sub1sp2(broken): unexpected overflow');
  }

  return { value: { kind: 'normal', sign, prec: p, exp, mant }, ternary };
}
