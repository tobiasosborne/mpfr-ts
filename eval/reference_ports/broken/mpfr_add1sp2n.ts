/**
 * reference_ports/broken/mpfr_add1sp2n.ts — deliberately-buggy mpfr_add1sp2n.
 *
 * **Multi-bug perturbation:**
 *   1. Use d = small.exp - large.exp (sign-flipped d).
 *   2. Use b.exp (not max) as the base.
 *   3. Invert the sign on the result.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';
import { mpfr_overflow } from '../../../src/ops/overflow.ts';

const EMAX_DEFAULT = (1n << 30n) - 1n;

function bitLen(x: bigint): bigint {
  if (x <= 0n) return 0n;
  return BigInt(x.toString(2).length);
}

export function mpfr_add1sp2n(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'add1sp2n(broken): non-normal');
  }
  if (b.prec !== 128n || c.prec !== 128n) {
    throw new MPFRError('EPREC', 'add1sp2n(broken): bad prec');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'add1sp2n(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'add1sp2n(broken): bad rnd');
  }

  const p = 128n;
  // BUG 3: invert sign.
  const sign: Sign = (-b.sign) as Sign;

  // BUG 1: don't swap; force large = b.
  const large = b;
  const small = c;
  // BUG 1: d uses wrong direction.
  let d = small.exp - large.exp;
  if (d < 0n) d = 0n;

  const S = (large.mant << d) + small.mant;
  if (S === 0n) {
    return { value: { kind: 'normal', sign, prec: p, exp: 0n, mant: 1n << (p - 1n) }, ternary: 0 };
  }
  const bl = bitLen(S);

  // BUG 2: use b.exp (not large.exp).
  const bxResult = b.exp + (bl - (p + d));

  let mant: bigint;
  let exp: bigint;
  let ternary: Ternary;
  if (bl <= p) {
    mant = S << (p - bl);
    exp = bxResult;
    ternary = 0;
  } else {
    const r = roundMantissa(S, bl, bxResult, p, sign, rnd);
    mant = r.mant;
    exp = r.exp;
    ternary = r.ternary;
  }

  if (exp > EMAX_DEFAULT) {
    return mpfr_overflow(p, rnd, sign);
  }

  return { value: { kind: 'normal', sign, prec: p, exp, mant }, ternary };
}
