/**
 * reference_ports/broken/mpfr_add1sp1n.ts — deliberately-buggy mpfr_add1sp1n.
 *
 * **Multi-bug perturbation:**
 *   1. Case A (equal exp): omit the `bx++` exponent bump.
 *   2. Case B1: drop the carry detection — always take the no-carry path.
 *   3. Case B2: invert rb (rb=1 iff d != 64).
 *   4. add_one_ulp: don't bump exp on mantissa overflow (leaves ap0 = 0
 *      which then violates schema or yields wrong value).
 *   5. RNDA: treated as RNDN (drops the unconditional add_one_ulp).
 *
 * Composite should drop well below 0.45.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { mpfr_overflow } from '../../../src/ops/overflow.ts';

const GMP_NUMB_BITS = 64n;
const LIMB_MASK_64 = (1n << GMP_NUMB_BITS) - 1n;
const LIMB_HIGHBIT = 1n << 63n;
const EMAX_DEFAULT = (1n << 30n) - 1n;

function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

function buildNormal(sign: Sign, exp: bigint, mant: bigint): MPFR {
  return { kind: 'normal', sign, prec: 64n, exp, mant };
}

export function mpfr_add1sp1n(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'add1sp1n(broken): non-normal');
  }
  if (b.prec !== 64n || c.prec !== 64n) {
    throw new MPFRError('EPREC', 'add1sp1n(broken): bad prec');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'add1sp1n(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'add1sp1n(broken): bad rnd');
  }

  const sign = b.sign;
  let bp0 = b.mant;
  let cp0 = c.mant;
  let bx = b.exp;
  let cx = c.exp;
  let ap0: bigint;
  let rb: bigint;
  let sb: bigint;

  if (bx === cx) {
    const fullSum = bp0 + cp0;
    const a0 = fullSum & LIMB_MASK_64;
    rb = a0 & 1n;
    ap0 = LIMB_HIGHBIT | (a0 >> 1n);
    // BUG 1: omit bx++
    sb = 0n;
  } else {
    if (bx < cx) {
      const tx = bx; bx = cx; cx = tx;
      const tp = bp0; bp0 = cp0; cp0 = tp;
    }
    const d = bx - cx;
    if (d < GMP_NUMB_BITS) {
      const a0_full = bp0 + (cp0 >> d);
      const a0_wrapped = a0_full & LIMB_MASK_64;
      sb = (cp0 << (GMP_NUMB_BITS - d)) & LIMB_MASK_64;
      // BUG 2: never detect carry — always no-carry path.
      ap0 = a0_wrapped;
      rb = sb & LIMB_HIGHBIT;
      sb = sb & ~LIMB_HIGHBIT & LIMB_MASK_64;
    } else {
      // BUG 3: invert rb.
      sb = (d !== GMP_NUMB_BITS || cp0 !== LIMB_HIGHBIT) ? 1n : 0n;
      ap0 = bp0;
      rb = (d !== GMP_NUMB_BITS) ? 1n : 0n;
    }
  }

  if (bx > EMAX_DEFAULT) {
    return mpfr_overflow(64n, rnd, sign);
  }

  if (rb === 0n && sb === 0n) {
    return { value: buildNormal(sign, bx, ap0), ternary: 0 };
  }

  let doAddOneUlp: boolean;
  if (rnd === 'RNDN' || rnd === 'RNDA') {
    // BUG 5: treat RNDA like RNDN (drops unconditional add-one).
    const lsb = ap0 & 1n;
    if (rb === 0n || (sb === 0n && lsb === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, sign)) {
    doAddOneUlp = false;
  } else {
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    const ternary: Ternary = (sign === 1 ? -1 : 1) as Ternary;
    return { value: buildNormal(sign, bx, ap0), ternary };
  }

  let newAp0 = (ap0 + 1n) & LIMB_MASK_64;
  // BUG 4: don't bump exp on overflow; rely on the post-clamp (ap0 = 0
  // which violates the schema → harness sees it as a throw).
  if (newAp0 === 0n) {
    // Leave it as 0 — this throws on validate().
    newAp0 = 0n;
  }
  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  return { value: buildNormal(sign, bx, newAp0), ternary };
}
