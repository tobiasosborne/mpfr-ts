/**
 * reference_ports/broken/mpfr_sub1sp3.ts — deliberately-buggy mpfr_sub1sp3.
 *
 * **Deliberately broken: omit the shift on the larger operand when
 * aligning** — same shape as the sub1sp2 broken port. Effect: every
 * case with `d > 0` (different exponents) produces a wrong result.
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

export function mpfr_sub1sp3(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'sub1sp3(broken): non-normal');
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', 'sub1sp3(broken): prec mismatch');
  }
  if (b.prec <= 128n || b.prec >= 192n) {
    throw new MPFRError('EPREC', 'sub1sp3(broken): prec out of range');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'sub1sp3(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'sub1sp3(broken): bad rnd');
  }

  const p = b.prec;
  const cmp = cmpMag(b, c);
  if (cmp === 0) {
    // BUG: always +0 on cancellation.
    return { value: posZero(p), ternary: 0 };
  }

  let large: MPFR, small: MPFR;
  let sign: Sign;
  if (cmp === 1) {
    large = b; small = c; sign = b.sign;
  } else {
    large = c; small = b; sign = -b.sign as Sign;
  }

  // BUG: forget the shift; subtract mantissas at raw scale.
  const D = large.mant - small.mant;

  if (D <= 0n) {
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
    const rounded = roundMantissa(D, bl, bxIn, p, sign, rnd);
    mant = rounded.mant;
    exp = rounded.exp;
    ternary = rounded.ternary;
  }

  if (exp < EMIN_DEFAULT) {
    return mpfr_underflow(p, rnd, sign);
  }

  if (exp > EMAX_DEFAULT) {
    throw new MPFRError('EPREC', 'sub1sp3(broken): unexpected overflow');
  }

  return { value: { kind: 'normal', sign, prec: p, exp, mant }, ternary };
}
