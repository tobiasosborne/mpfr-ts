/**
 * ops/floor.ts — pure-TS port of MPFR's `mpfr_floor`.
 *
 * Round an {@link MPFR} value toward `-∞` (floor) to the largest
 * prec-representable integer <= x. Returns canonical `{value, ternary}`.
 *
 * `floor(2.7) = 2`, `floor(-2.7) = -3` (more negative!),
 * `floor(0.3) = +0`, `floor(-0.3) = -1`.
 *
 * When the prec is too low to represent the floor exactly, the result
 * is the largest prec-rep value <= mathematical floor.
 *
 * Example: `floor(-7, prec=2) = -8` (because -7 isn't prec-2-rep; -8 is
 * the next one further toward -∞).
 *
 * C signature
 * -----------
 *
 *   int mpfr_floor(mpfr_t r, mpfr_srcptr u);
 *
 *   mpfr_floor defers to mpfr_rint(r, u, MPFR_RNDD).
 *   Ref: mpfr/src/rint.c L341–L344.
 *
 * TS signature
 * ------------
 *
 *   mpfr_floor(x: MPFR, prec: bigint): Result;
 *
 * Algorithm
 * ---------
 *
 * Specials propagate.
 *
 * For normal x:
 *
 *   1. |x| < 1 (`x.exp <= 0`):
 *      - sign +1: floor magnitude → 0; result is +0 (sign preserved).
 *      - sign -1: floor of negative fractional → -1.
 *
 *   2. |x| >= 1 (`x.exp >= 1`): unified mpfr_rint with `rnd_away =
 *      (sign < 0)`. RNDD on positive truncates; RNDD on negative
 *      magnitude-up (= toward -∞).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/rint.c L341–L344 — wrapper.
 *   - mpfr/src/rint.c L67–L72 — `rnd_away = sign < 0` for RNDD.
 *   - src/ops/ceil.ts — symmetric sibling.
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
 * |x|>=1 floor path. RNDD: rnd_away = (sign < 0). Increment magnitude
 * iff any bit was dropped AND sign is negative.
 */
function floorIntegral(
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

  // RNDD: increment magnitude iff negative.
  const increment = sign === -1;
  if (!increment) {
    return {
      mant: truncMant,
      exp: xExp,
      sign,
      // Positive truncated: rounded < exact → ternary -1.
      ternary: -1,
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
  // Negative incremented (magnitude up): rounded magnitude > exact
  // magnitude. Signed value: more negative. So rounded < exact → -1.
  return { mant, exp, sign, ternary: -1 };
}

/**
 * Floor an MPFR value toward -∞ at the target precision.
 *
 * @mpfrName mpfr_floor
 *
 * @example
 *   floor(setD(2.7, 53n, 'RNDN').value, 53n);
 *     // → {value: 2, ternary: -1}
 *   floor(setD(-2.7, 53n, 'RNDN').value, 53n);
 *     // → {value: -3, ternary: -1}
 *   floor(setD(0.3, 53n, 'RNDN').value, 53n);
 *     // → {value: +0, ternary: -1}
 *   floor(setD(-0.3, 53n, 'RNDN').value, 53n);
 *     // → {value: -1, ternary: -1}
 */
export function mpfr_floor(x: MPFR, prec: bigint): Result {
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
      `mpfr_floor: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  // |x| < 1
  if (x.exp <= 0n) {
    if (x.sign === 1) {
      // floor(+frac) = +0.
      const value: MPFR = { kind: 'zero', sign: 1, prec, exp: 0n, mant: 0n };
      return { value, ternary: -1 };
    }
    // floor(-frac) = -1.
    const value: MPFR = {
      kind: 'normal',
      sign: -1,
      prec,
      exp: 1n,
      mant: 1n << (prec - 1n),
    };
    return { value, ternary: -1 };
  }

  const r = floorIntegral(x, prec);
  const value: MPFR = {
    kind: 'normal',
    sign: r.sign,
    prec,
    exp: r.exp,
    mant: r.mant,
  };
  return { value, ternary: r.ternary };
}
