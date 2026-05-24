/**
 * reference_ports/broken/mpfr_add1sp3.ts — deliberately-buggy mpfr_add1sp3.
 *
 * **Multi-bug perturbation:**
 *   1. Skip shifting large.mant by d (treat operands as same exp).
 *   2. Force RNDA to act like RNDZ (drop unconditional add-one).
 *   3. Use srcPrec = p instead of bl in roundMantissa (off-by-many on
 *      most cases, but only when bl > p).
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';
import { mpfr_overflow } from '../../../src/ops/overflow.ts';

const EMAX_DEFAULT = (1n << 30n) - 1n;

function bitLen(x: bigint): bigint {
  if (x <= 0n) return 0n;
  return BigInt(x.toString(2).length);
}

export function mpfr_add1sp3(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'add1sp3(broken): non-normal');
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', 'add1sp3(broken): prec mismatch');
  }
  const p = b.prec;
  if (p <= 128n || p >= 192n) {
    throw new MPFRError('EPREC', 'add1sp3(broken): bad prec');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'add1sp3(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'add1sp3(broken): bad rnd');
  }

  const sign = b.sign;
  let large: MPFR, small: MPFR;
  if (b.exp >= c.exp) {
    large = b; small = c;
  } else {
    large = c; small = b;
  }
  // d unused due to BUG 1 — but compute for size-bound expr below.
  const d = large.exp - small.exp;

  // BUG 1: don't shift large.mant by d.
  const S = large.mant + small.mant;
  if (S === 0n) {
    // Defensive — shouldn't happen for same-sign positives.
    return { value: { kind: 'normal', sign, prec: p, exp: 0n, mant: 1n << (p - 1n) }, ternary: 0 };
  }
  const bl = bitLen(S);
  const bxResult = large.exp + (bl - (p + d));

  let mant: bigint;
  let exp: bigint;
  let ternary: Ternary;
  if (bl <= p) {
    mant = S << (p - bl);
    exp = bxResult;
    ternary = 0;
  } else {
    // BUG 3 (when RNDA): pretend RNDZ for this path.
    const r2 = rnd === 'RNDA' ? 'RNDZ' : rnd;
    const r = roundMantissa(S, bl, bxResult, p, sign, r2);
    mant = r.mant;
    exp = r.exp;
    ternary = r.ternary;
  }

  if (exp > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
  }

  return { value: { kind: 'normal', sign, prec: p, exp, mant }, ternary };
}
