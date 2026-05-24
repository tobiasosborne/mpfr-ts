/**
 * reference_ports/broken/mpfr_sub1sp1n.ts — deliberately-buggy mpfr_sub1sp1n.
 *
 * **Deliberately broken: skip the leading-zero normalisation in Case A
 * (equal-exponents exact subtraction).** When `bx == cx`, the C
 * algorithm computes `a0 = bp0 - cp0` and then `count_leading_zeros(cnt,
 * a0); ap[0] = a0 << cnt; bx -= cnt`. This broken port skips the
 * `cnt` computation: it just stores `a0` directly and leaves `bx`
 * unchanged. Effect: every Case A non-cancelling case has a non-MSB-
 * normalised result (mantissa < 2^63), which fails schema validation
 * (`mant_aligned` check) — but the broken-port "fudge" preserves the
 * MSB so the value is structurally valid but numerically wrong (off by
 * a power of 2 in the exponent).
 *
 * Composite drops below 0.55: every Case A near-cancellation case
 * fails (most "edge", several "adversarial"), every Case B1 / B2 case
 * stays correct.
 *
 * NOT used in production.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { MPFRError, posZero, negZero } from '../../../src/core.ts';
import { mpfr_underflow } from '../../../src/ops/underflow.ts';

const GMP_NUMB_BITS = 64n;
const LIMB_MASK_64 = (1n << GMP_NUMB_BITS) - 1n;
const LIMB_HIGHBIT = 1n << 63n;
const EMIN_DEFAULT = -((1n << 30n) - 1n);

function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  return false;
}

function neg64(x: bigint): bigint {
  return (-x) & LIMB_MASK_64;
}

export function mpfr_sub1sp1n(b: MPFR, c: MPFR, rnd: RoundingMode): Result {
  if (b.kind !== 'normal' || c.kind !== 'normal') {
    throw new MPFRError('EPREC', 'sub1sp1n(broken): non-normal');
  }
  if (b.prec !== 64n || c.prec !== 64n) {
    throw new MPFRError('EPREC', 'sub1sp1n(broken): prec must be 64');
  }
  if (b.sign !== c.sign) {
    throw new MPFRError('EPREC', 'sub1sp1n(broken): sign mismatch');
  }
  const validRnd: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];
  if (!validRnd.includes(rnd)) {
    throw new MPFRError('EROUND', 'sub1sp1n(broken): bad rnd');
  }

  const p = 64n;
  let bp0 = b.mant;
  let cp0 = c.mant;
  let bx = b.exp;
  let cx = c.exp;
  let sign: Sign = b.sign;
  let ap0: bigint;
  let rb: bigint;
  let sb: bigint;

  if (bx === cx) {
    let a0 = (bp0 - cp0) & LIMB_MASK_64;
    if (a0 === 0n) {
      return { value: rnd === 'RNDD' ? negZero(p) : posZero(p), ternary: 0 };
    }
    if (bp0 < cp0) {
      sign = -b.sign as Sign;
      a0 = neg64(a0);
    }
    // BUG: skip normalisation. Leave a0 unshifted.
    // For results where a0 already has MSB set (large enough), this works;
    // but for cancellation cases where bp0 and cp0 are close, a0 is small
    // and the result is wrong.
    if ((a0 & LIMB_HIGHBIT) === 0n) {
      // Restore MSB to pass schema validation, but value is wrong.
      ap0 = a0 | LIMB_HIGHBIT;
    } else {
      ap0 = a0;
    }
    rb = 0n;
    sb = 0n;
  } else {
    if (bx < cx) {
      const tx = bx; bx = cx; cx = tx;
      const tp = bp0; bp0 = cp0; cp0 = tp;
      sign = -b.sign as Sign;
    }
    const d = bx - cx;
    if (d < GMP_NUMB_BITS) {
      // BUG: forget to subtract the borrow from sb. Drop the
      // (sb != 0 ? 1 : 0) borrow term and the leading-zero normalisation.
      sb = neg64((cp0 << (GMP_NUMB_BITS - d)) & LIMB_MASK_64);
      let a0 = (bp0 - (cp0 >> d)) & LIMB_MASK_64;
      if (a0 === 0n) {
        bx -= GMP_NUMB_BITS;
        ap0 = LIMB_HIGHBIT;
        rb = 0n;
        sb = 0n;
      } else {
        // BUG: no clz normalisation; just take the bottom bits as rb/sb.
        rb = sb & LIMB_HIGHBIT;
        sb = sb & (LIMB_HIGHBIT - 1n);
        if ((a0 & LIMB_HIGHBIT) === 0n) {
          ap0 = a0 | LIMB_HIGHBIT;
        } else {
          ap0 = a0;
        }
      }
    } else {
      if (bp0 > LIMB_HIGHBIT) {
        rb = (d > GMP_NUMB_BITS || cp0 === LIMB_HIGHBIT) ? 1n : 0n;
        sb = (d > GMP_NUMB_BITS || cp0 !== LIMB_HIGHBIT) ? 1n : 0n;
        ap0 = bp0 - 1n;
      } else {
        bx -= 1n;
        if (d === GMP_NUMB_BITS && cp0 > LIMB_HIGHBIT) {
          const negc = neg64(cp0);
          rb = negc >= (LIMB_HIGHBIT >> 1n) ? 1n : 0n;
          sb = (negc << 2n) & LIMB_MASK_64;
          ap0 = neg64(1n << 1n);
        } else {
          const condE = d > GMP_NUMB_BITS + 1n;
          const condC = d === GMP_NUMB_BITS + 1n && cp0 === LIMB_HIGHBIT;
          rb = (condE || condC) ? 1n : 0n;
          const condD = d === GMP_NUMB_BITS + 1n && cp0 > LIMB_HIGHBIT;
          sb = (condE || condD) ? 1n : 0n;
          ap0 = neg64(1n);
        }
      }
    }
  }

  if (bx < EMIN_DEFAULT) {
    let effRnd: RoundingMode = rnd;
    if (rnd === 'RNDN' && (bx < EMIN_DEFAULT - 1n || ap0 === LIMB_HIGHBIT)) {
      effRnd = 'RNDZ';
    }
    return mpfr_underflow(p, effRnd, sign);
  }

  if (rb === 0n && sb === 0n) {
    return { value: { kind: 'normal', sign, prec: p, exp: bx, mant: ap0 }, ternary: 0 };
  }

  let doAddOneUlp: boolean;
  if (rnd === 'RNDN') {
    if (rb === 0n || (sb === 0n && (ap0 & 1n) === 0n)) {
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
    return { value: { kind: 'normal', sign, prec: p, exp: bx, mant: ap0 }, ternary };
  }

  let newAp0 = (ap0 + 1n) & LIMB_MASK_64;
  let newBx = bx;
  if (newAp0 === 0n) {
    newAp0 = LIMB_HIGHBIT;
    newBx = bx + 1n;
  }
  const ternary: Ternary = (sign === 1 ? 1 : -1) as Ternary;
  return { value: { kind: 'normal', sign, prec: p, exp: newBx, mant: newAp0 }, ternary };
}
