/**
 * reference_ports/broken/mpfr_add1sp2.ts — deliberately-buggy mpfr_add1sp2.
 *
 * **Multi-bug perturbation:**
 *   1. Subtract instead of add: S = (large.mant << d) - small.mant.
 *      Every case produces a wrong value.
 *   2. Use min exp instead of large exp for the bx base.
 *   3. Force ternary = 0 always (drops half the rounding-mode signal).
 *
 * Composite should drop well below 0.45.
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

export function mpfr_add1sp2(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'add1sp2(broken): non-normal');
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', 'add1sp2(broken): prec mismatch');
  }
  const p = b.prec;
  if (p <= 64n || p >= 128n) {
    throw new MPFRError('EPREC', 'add1sp2(broken): bad prec');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'add1sp2(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'add1sp2(broken): bad rnd');
  }

  const sign = b.sign;
  let large: MPFR, small: MPFR;
  if (b.exp >= c.exp) {
    large = b; small = c;
  } else {
    large = c; small = b;
  }
  const d = large.exp - small.exp;

  // BUG 1: subtract instead of add.
  const S0 = (large.mant << d) - small.mant;
  // BUG 2: small.exp - 1 instead of derivation from bitLen.
  if (S0 <= 0n) {
    // fallback to large (deliberately drop bitLen recomputation)
    return { value: { kind: 'normal', sign, prec: p, exp: large.exp, mant: large.mant }, ternary: 0 };
  }
  const bl = bitLen(S0);
  const bxResult = small.exp - 1n;  // BUG: wrong base

  let mant: bigint;
  let exp: bigint;
  if (bl <= p) {
    mant = S0 << (p - bl);
    exp = bxResult;
  } else {
    const r = roundMantissa(S0, bl, bxResult, p, sign, rnd);
    mant = r.mant;
    exp = r.exp;
  }

  if (exp > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
  }

  // BUG 3: always ternary=0.
  const ternary: Ternary = 0;
  return { value: { kind: 'normal', sign, prec: p, exp, mant }, ternary };
}
