/**
 * ops/trunc.ts — pure-TS port of MPFR's `mpfr_trunc`.
 *
 * Round an {@link MPFR} value toward zero (truncate) to the largest
 * (in magnitude) prec-representable integer with magnitude <= |x|,
 * returning the canonical `{value, ternary}` shape from src/core.ts.
 *
 * `trunc(2.7) = 2`, `trunc(-2.7) = -2`, `trunc(0.99) = +0`,
 * `trunc(-0.99) = -0`. When the target prec is too low to represent the
 * truncated integer exactly, the result is the largest prec-rep value
 * with magnitude <= |trunc(x)|, e.g. `trunc(7, prec=2) = 6` (since 7
 * isn't prec-2-representable; 6 is the next one down).
 *
 * C signature
 * -----------
 *
 *   int mpfr_trunc(mpfr_t r, mpfr_srcptr u);
 *
 *   mpfr_trunc defers to mpfr_rint(r, u, MPFR_RNDZ) —
 *   mpfr/src/rint.c L325–L328.
 *
 * TS signature
 * ------------
 *
 *   mpfr_trunc(x: MPFR, prec: bigint): Result;
 *
 *   - takes `prec` as an explicit positional argument;
 *   - no rounding-mode parameter (direction is implicit: toward zero);
 *   - returns the immutable {@link Result} from src/core.ts.
 *
 * Algorithm
 * ---------
 *
 * Specials propagate: NaN→NaN, ±Inf→±Inf, ±0→±0 (sign preserved).
 *
 * For normal x:
 *
 *   1. |x| < 1 (`x.exp <= 0`): trunc magnitude → 0; signed zero is
 *      preserved (sign-of-x). Ternary is sign(0 - x) i.e. -1 if x > 0,
 *      +1 if x < 0.
 *
 *   2. |x| >= 1 (`x.exp >= 1`): unified mpfr_rint algorithm with
 *      `rnd_away = 0` (RNDZ: never increment). The result is the top
 *      `prec` bits of x's mantissa frame, masking away any fractional
 *      bits. See `truncIntegral` below.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/rint.c L325–L328 — wrapper.
 *   - mpfr/src/rint.c L35–L304 — mpfr_rint engine.
 *   - mpfr/tests/ttrunc.c — coverage tests.
 *   - src/ops/round.ts, src/ops/ceil.ts, src/ops/floor.ts — siblings.
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
 * |x|>=1 truncation path. RNDZ semantics: never increment. The result
 * mantissa is the top `prec` bits of x's integer part; result exp is
 * x.exp (or x.exp+1 only in the never-here carry path). Exact iff x
 * is itself a prec-representable integer.
 */
function truncIntegral(
  x: MPFR,
  prec: bigint,
): { mant: bigint; exp: bigint; sign: Sign; ternary: -1 | 0 | 1 } {
  const xExp = x.exp;
  const xPrec = x.prec;
  const xMant = x.mant;
  const sign: Sign = x.sign;

  // Compute the truncated mantissa AND track whether anything was
  // dropped (used to decide ternary).
  let truncMant: bigint;
  let droppedAny: boolean;

  if (xExp >= prec) {
    // Top prec bits of the integer part.
    if (xPrec >= prec) {
      const shift = xPrec - prec;
      truncMant = xMant >> shift;
      droppedAny = shift > 0n && (xMant & ((1n << shift) - 1n)) !== 0n;
    } else {
      // xExp > xPrec: x is already an integer with implicit trailing
      // zeros. Pad left to prec.
      truncMant = xMant << (prec - xPrec);
      droppedAny = false;
    }
  } else {
    // Regime B: integer part has xExp bits, pad to prec.
    if (xPrec > xExp) {
      const fracBitsCount = xPrec - xExp;
      const intAbs = xMant >> fracBitsCount;
      droppedAny = (xMant & ((1n << fracBitsCount) - 1n)) !== 0n;
      truncMant = intAbs << (prec - xExp);
    } else {
      // xPrec <= xExp < prec: x is an integer; pad mantissa to prec.
      truncMant = xMant << (prec - xPrec);
      droppedAny = false;
    }
  }

  if (!droppedAny) {
    return {
      mant: truncMant,
      exp: xExp,
      sign,
      ternary: 0,
    };
  }

  // RNDZ: never increment.
  return {
    mant: truncMant,
    exp: xExp,
    sign,
    // Truncated: rounded magnitude < exact. ternary follows sign.
    ternary: sign === 1 ? -1 : 1,
  };
}

/**
 * Truncate an MPFR value toward zero at the target precision.
 *
 * @mpfrName mpfr_trunc
 *
 * @param x     the operand. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 *
 * @returns     `{value, ternary}`.
 *
 * @throws {MPFRError} `EPREC` on bad precision. NaN/Inf is NOT an error.
 *
 * @example
 *   trunc(setD(2.7, 53n, 'RNDN').value, 53n);
 *     // → {value: 2, ternary: -1}
 *   trunc(setD(-2.7, 53n, 'RNDN').value, 53n);
 *     // → {value: -2, ternary: +1}  (rounded > exact, less negative)
 *   trunc(setD(0.99, 53n, 'RNDN').value, 53n);
 *     // → {value: +0, ternary: -1}
 *   trunc(setD(-0.99, 53n, 'RNDN').value, 53n);
 *     // → {value: -0, ternary: +1}
 */
export function mpfr_trunc(x: MPFR, prec: bigint): Result {
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
      `mpfr_trunc: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  // |x| < 1: trunc → 0 with x's sign preserved.
  if (x.exp <= 0n) {
    const value: MPFR =
      x.sign === 1
        ? { kind: 'zero', sign: 1, prec, exp: 0n, mant: 0n }
        : { kind: 'zero', sign: -1, prec, exp: 0n, mant: 0n };
    return { value, ternary: x.sign === 1 ? -1 : 1 };
  }

  const r = truncIntegral(x, prec);
  const value: MPFR = {
    kind: 'normal',
    sign: r.sign,
    prec,
    exp: r.exp,
    mant: r.mant,
  };
  return { value, ternary: r.ternary };
}
