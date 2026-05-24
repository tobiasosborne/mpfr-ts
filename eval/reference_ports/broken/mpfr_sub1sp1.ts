/**
 * reference_ports/broken/mpfr_sub1sp1.ts — deliberately-buggy mpfr_sub1sp1.
 *
 * **Multi-bug perturbation:**
 *   1. Drop the leading-zero normalisation in Case A (equal exponents).
 *      Result: any cancelling case with `a0 < HIGHBIT << sh` stores the
 *      wrong (un-normalised) value.
 *   2. In Case B1, omit the borrow contribution from `sb` (use `bp0 - (cp0 >> d)`).
 *   3. In Case B1, skip clz normalisation entirely.
 *   4. In Case B2, when bp0 == HIGHBIT, set rb = 0 regardless (drops one rb branch).
 *   5. In the rounding stage, treat RNDA the same as RNDZ (truncates instead of
 *      adds one ulp).
 *
 * Composite should drop well below 0.45 — five overlapping perturbations
 * across Cases A, B1, B2, and the rounding layer.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { MPFRError, posZero } from '../../../src/core.ts';
import { mpfr_underflow } from '../../../src/ops/underflow.ts';

const GMP_NUMB_BITS = 64n;
const LIMB_MASK_64 = (1n << GMP_NUMB_BITS) - 1n;
const LIMB_HIGHBIT = 1n << 63n;
const EMIN_DEFAULT = -((1n << 30n) - 1n);

function lmbMask(s: bigint): bigint {
  if (s === 0n) return 0n;
  if (s >= 64n) return LIMB_MASK_64;
  return (1n << s) - 1n;
}

function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

function buildNormal(p: bigint, sh: bigint, sign: Sign, exp: bigint, ap0: bigint): MPFR {
  const mant = ap0 >> sh;
  return { kind: 'normal', sign, prec: p, exp, mant };
}

export function mpfr_sub1sp1(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'sub1sp1(broken): non-normal');
  }
  if (b.prec !== c.prec) {
    throw new MPFRError('EPREC', 'sub1sp1(broken): prec mismatch');
  }
  if (b.prec < 1n || b.prec > 63n) {
    throw new MPFRError('EPREC', 'sub1sp1(broken): prec out of range');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'sub1sp1(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'sub1sp1(broken): bad rnd');
  }

  const p = b.prec;
  const sh = GMP_NUMB_BITS - p;
  let bp0 = b.mant << sh;
  let cp0 = c.mant << sh;
  let bx = b.exp;
  let cx = c.exp;
  let resultSign: Sign = b.sign;
  let ap0: bigint;
  let rb: bigint;
  let sb: bigint;

  if (bx === cx) {
    if (bp0 === cp0) {
      // BUG 1a: always return +0 (drop signed-zero).
      return { value: posZero(p), ternary: 0 };
    }
    let a0: bigint;
    if (cp0 > bp0) {
      // BUG 1b: keep b's sign on borrow.
      a0 = cp0 - bp0;
      resultSign = b.sign;
    } else {
      a0 = bp0 - cp0;
      resultSign = b.sign;
    }
    // BUG 1c: ALWAYS shift left by 1 instead of clz amount.
    ap0 = ((a0 << 1n) | LIMB_HIGHBIT) & LIMB_MASK_64;
    // BUG 1d: don't decrement bx.
    rb = 0n;
    sb = 0n;
  } else {
    // BUG 2a: don't swap (treat operands as already ordered).
    resultSign = b.sign;
    // BUG 2b: use |d| from raw subtraction (always positive).
    let d = bx - cx;
    if (d < 0n) d = -d;
    const mask = lmbMask(sh);

    if (d < GMP_NUMB_BITS) {
      const shift = GMP_NUMB_BITS - d;
      const shifted_cp = (cp0 << shift) & LIMB_MASK_64;
      sb = shifted_cp === 0n ? 0n : ((1n << GMP_NUMB_BITS) - shifted_cp) & LIMB_MASK_64;

      // BUG 3: add instead of subtract (turns sub into add).
      const cp_shifted = cp0 >> d;
      let a0 = (bp0 + cp_shifted) & LIMB_MASK_64;
      if (a0 === 0n) a0 = 1n;

      if ((a0 & LIMB_HIGHBIT) === 0n) {
        ap0 = a0 | LIMB_HIGHBIT;
      } else {
        ap0 = a0;
      }
      rb = ap0 & (1n << (sh - 1n));
      sb = sb | ((ap0 & mask) ^ rb);
      ap0 = ap0 & ~mask;
    } else {
      if (bp0 > LIMB_HIGHBIT) {
        ap0 = bp0 - (1n << sh);
        rb = 1n;
      } else {
        // BUG 4: rb = 0 regardless.
        rb = 0n;
        ap0 = (~mask) & LIMB_MASK_64;
        bx = bx - 1n;
      }
      sb = 1n;
    }
  }

  if (bx < EMIN_DEFAULT) {
    let effRnd: RoundingMode = rnd;
    if (rnd === 'RNDN' && (bx < EMIN_DEFAULT - 1n || ap0 === LIMB_HIGHBIT)) {
      effRnd = 'RNDZ';
    }
    return mpfr_underflow(p, effRnd, resultSign);
  }

  if (rb === 0n && sb === 0n) {
    return { value: buildNormal(p, sh, resultSign, bx, ap0), ternary: 0 };
  }

  let doAddOneUlp: boolean;
  if (rnd === 'RNDN') {
    const lsbBit = ap0 & (1n << sh);
    if (rb === 0n || (sb === 0n && lsbBit === 0n)) {
      doAddOneUlp = false;
    } else {
      doAddOneUlp = true;
    }
  } else if (isLikeRNDZ(rnd, resultSign)) {
    doAddOneUlp = false;
  } else if (rnd === 'RNDA') {
    // BUG 5: treat RNDA as RNDZ.
    doAddOneUlp = false;
  } else {
    doAddOneUlp = true;
  }

  if (!doAddOneUlp) {
    const ternary: Ternary = (resultSign === 1 ? -1 : 1) as Ternary;
    return { value: buildNormal(p, sh, resultSign, bx, ap0), ternary };
  }

  let newAp0 = (ap0 + (1n << sh)) & LIMB_MASK_64;
  let newBx = bx;
  if (newAp0 === 0n) {
    newAp0 = LIMB_HIGHBIT;
    newBx = bx + 1n;
  }
  const ternary: Ternary = (resultSign === 1 ? 1 : -1) as Ternary;
  return { value: buildNormal(p, sh, resultSign, newBx, newAp0), ternary };
}
