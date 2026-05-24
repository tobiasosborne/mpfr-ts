/**
 * ops/ceil.ts — pure-TS port of MPFR's `mpfr_ceil`.
 *
 * Round an {@link MPFR} value toward `+∞` (ceiling) to the smallest
 * prec-representable integer >= x. Returns canonical `{value, ternary}`.
 *
 * `ceil(2.7) = 3`, `ceil(-2.7) = -2`, `ceil(0.3) = 1`,
 * `ceil(-0.3) = -0` (sign preserved).
 *
 * When prec is too low to represent the ceil exactly, the result is the
 * smallest prec-rep value >= mathematical ceil.
 *
 * C signature
 * -----------
 *
 *   int mpfr_ceil(mpfr_t r, mpfr_srcptr u);
 *
 *   mpfr_ceil defers to mpfr_rint(r, u, MPFR_RNDU).
 *   Ref: mpfr/src/rint.c L333–L336.
 *
 * TS signature
 * ------------
 *
 *   mpfr_ceil(x: MPFR, prec: bigint): Result;
 *
 * Algorithm — symmetric to floor with sign flipped.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/rint.c L333–L336 — wrapper.
 *   - mpfr/src/rint.c L67–L72 — `rnd_away = sign > 0` for RNDU.
 *   - src/ops/floor.ts — symmetric sibling.
 *   - src/core.ts — locked types.
 */

import type { MPFR, Result, Sign } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
} from '../core.ts';

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

/**
 * |x|>=1 ceil path. RNDU: rnd_away = (sign > 0). Increment magnitude
 * iff any bit was dropped AND sign is positive.
 */
function ceilIntegral(
  x: MPFR,
  prec: bigint,
): { mant: bigint; exp: bigint; sign: Sign; ternary: -1 | 0 | 1 } {
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
    return { mant: truncMant, exp: xExp, sign, ternary: 0 };
  }

  // RNDU: increment magnitude iff positive.
  const increment = sign === 1;
  if (!increment) {
    return {
      mant: truncMant,
      exp: xExp,
      sign,
      // Negative truncated (magnitude down): rounded magnitude < exact
      // magnitude → signed rounded > exact (less negative) → ternary +1.
      ternary: 1,
    };
  }
  // Increment by 1 ulp-of-the-integer (see round.ts for the rationale).
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
  // Positive incremented: rounded > exact → ternary +1.
  return { mant, exp, sign, ternary: 1 };
}

/**
 * Ceil an MPFR value toward +∞ at the target precision.
 *
 * @mpfrName mpfr_ceil
 *
 * @example
 *   ceil(setD(2.7, 53n, 'RNDN').value, 53n);
 *     // → {value: 3, ternary: +1}
 *   ceil(setD(-2.7, 53n, 'RNDN').value, 53n);
 *     // → {value: -2, ternary: +1}
 *   ceil(setD(0.3, 53n, 'RNDN').value, 53n);
 *     // → {value: 1, ternary: +1}
 *   ceil(setD(-0.3, 53n, 'RNDN').value, 53n);
 *     // → {value: -0, ternary: +1}
 */
export function mpfr_ceil(x: MPFR, prec: bigint): Result {
  validatePrec(prec);

  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }
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
    throw new MPFRError(
      'EPREC',
      `mpfr_ceil: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  // |x| < 1
  if (x.exp <= 0n) {
    if (x.sign === 1) {
      // ceil(+frac) = +1.
      const value: MPFR = {
        kind: 'normal',
        sign: 1,
        prec,
        exp: 1n,
        mant: 1n << (prec - 1n),
      };
      return { value, ternary: 1 };
    }
    // ceil(-frac) = -0.
    const value: MPFR = { kind: 'zero', sign: -1, prec, exp: 0n, mant: 0n };
    return { value, ternary: 1 };
  }

  const r = ceilIntegral(x, prec);
  const value: MPFR = {
    kind: 'normal',
    sign: r.sign,
    prec,
    exp: r.exp,
    mant: r.mant,
  };
  return { value, ternary: r.ternary };
}
