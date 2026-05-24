/**
 * ops/div_2si.ts — pure-TS port of MPFR's `mpfr_div_2si`.
 *
 * Divide an {@link MPFR} value by `2^e` — equivalently, multiply by
 * `2^(-e)`. The mantissa is unchanged; only the binary exponent shifts
 * by `-e`. If the caller-supplied `prec` differs from `x.prec` the
 * mantissa is re-fitted via the shared rounding substrate.
 *
 * C signature
 * -----------
 *
 *   int mpfr_div_2si(mpfr_t rop, mpfr_srcptr op, long e, mpfr_rnd_t rnd);
 *
 *   Ref: mpfr/src/div_2si.c L24–L59. Same shape as mul_2si but with the
 *   exponent shift sign flipped (exp - n instead of exp + n).
 *
 * TS divergence (same as mul_2si)
 * -------------------------------
 *
 * Omits the C emax/emin range check; the locked schema has no exponent
 * cap on a `normal` MPFR.
 *
 * TS signature
 * ------------
 *
 *   mpfr_div_2si(x: MPFR, e: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 *   Identical to mul_2si except the final exp is `postExp - e` rather
 *   than `postExp + e`. Singulars propagate. Refit the mantissa to
 *   `prec` first (via padding or roundMantissa), then subtract `e`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/div_2si.c L24–L59 — C reference.
 *   - src/ops/mul_2si.ts — sibling; identical structure with sign flip.
 *   - src/internal/mpfr/round_raw.ts — substrate.
 *   - src/core.ts — locked schema.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
  validate,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

function validateArgs(
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof e !== 'bigint') {
    throw new MPFRError('EPREC', `e must be bigint, got ${typeof e}`);
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Compute `x / 2^e` = `x * 2^(-e)` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_div_2si
 *
 * @param x     the input MPFR. Must pass {@link validate}.
 * @param e     the (possibly negative) power of 2 to divide by, as a bigint.
 * @param prec  output precision in bits.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` / `EROUND` on bad inputs.
 *
 * @example
 *   const a = mpfr_set_si(12n, 53n, 'RNDN').value;
 *   mpfr_div_2si(a, 2n, 53n, 'RNDN');   // value = 3, ternary 0
 *   mpfr_div_2si(a, -2n, 53n, 'RNDN');  // value = 48, ternary 0
 */
export function mpfr_div_2si(
  x: MPFR,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(e, prec, rnd);
  validate(x);

  switch (x.kind) {
    case 'nan':
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      return { value: x.sign === 1 ? posInf(prec) : negInf(prec), ternary: 0 };
    case 'zero':
      return {
        value: x.sign === 1 ? posZero(prec) : negZero(prec),
        ternary: 0,
      };
    case 'normal':
      break;
  }

  let postExp: bigint;
  let postMant: bigint;
  let ternary: -1 | 0 | 1;

  if (prec >= x.prec) {
    postMant = x.mant << (prec - x.prec);
    postExp = x.exp;
    ternary = 0;
  } else {
    const { mant, exp, ternary: tr } = roundMantissa(
      x.mant,
      x.prec,
      x.exp,
      prec,
      x.sign,
      rnd,
    );
    postMant = mant;
    postExp = exp;
    ternary = tr;
  }

  const value: MPFR = {
    kind: 'normal',
    sign: x.sign,
    prec,
    exp: postExp - e,
    mant: postMant,
  };
  return { value, ternary };
}
