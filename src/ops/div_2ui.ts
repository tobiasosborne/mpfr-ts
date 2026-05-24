/**
 * ops/div_2ui.ts — pure-TS port of MPFR's `mpfr_div_2ui`.
 *
 * Divide an {@link MPFR} value by `2^n` where `n` is a non-negative
 * integer. The mantissa is unchanged; only the binary exponent shifts
 * by `-n`. If the caller-supplied `prec` differs from `x.prec` the
 * mantissa is re-fitted via the shared rounding substrate.
 *
 * This is the unsigned specialisation of {@link mpfr_div_2si}; the C
 * reference (mpfr/src/div_2ui.c L24-L68) is structurally identical to
 * div_2si modulo the type of `n` (unsigned long vs signed long) and a
 * one-sided underflow check.
 *
 * C signature
 * -----------
 *
 *   int mpfr_div_2ui(mpfr_ptr y, mpfr_srcptr x, unsigned long int n,
 *                    mpfr_rnd_t rnd_mode);
 *
 *   Body (mpfr/src/div_2ui.c L24-L68):
 *
 *     if (n == 0 || MPFR_IS_SINGULAR(x)) return mpfr_set(y, x, rnd_mode);
 *     else {
 *         exp = MPFR_GET_EXP(x);
 *         MPFR_SETRAW(inexact, y, x, exp, rnd_mode);
 *         diffexp = (mpfr_uexp_t)exp - (mpfr_uexp_t)(__gmpfr_emin - 1);
 *         if (n >= diffexp) {
 *             // underflow
 *             return mpfr_underflow(y, rnd_mode, MPFR_SIGN(y));
 *         }
 *         MPFR_SET_EXP(y, exp - (mpfr_exp_t)n);
 *     }
 *     return inexact;
 *
 *   Ref: mpfr/src/div_2ui.c L24-L68.
 *
 * TS divergence (same as div_2si)
 * --------------------------------
 *
 * Omits the C emin underflow range check; the locked schema has no
 * exponent lower bound on a `normal` MPFR — `exp` is an arbitrary
 * bigint. Callers wanting saturating semantics compose with a downstream
 * `check_range` op. Mirrors the mpfr_div_2si policy
 * (src/ops/div_2si.ts §divergence_from_c).
 *
 * TS signature
 * ------------
 *
 *   mpfr_div_2ui(x: MPFR, n: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - `n` is a non-negative `bigint` (matching `set_ui`'s convention).
 *     Out-of-range (negative or > ULONG_MAX) throws `EPREC`.
 *   - `prec` is positional; the value is refitted to it.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `n`, `prec`, `rnd`, structural validity of `x`.
 *   2. NaN: return `{value: NAN_VALUE, ternary: 0}`.
 *   3. ±Inf / ±0: return the singular value at the caller-supplied
 *      precision with sign preserved.
 *   4. Normal: refit the mantissa to `prec` (pad or roundMantissa),
 *      then subtract `n` from the post-refit exponent.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/div_2ui.c L24-L68 — C reference.
 *   - src/ops/div_2si.ts — sibling signed port; this is the unsigned
 *     specialisation (n >= 0n).
 *   - src/ops/mul_2ui.ts — sibling op (positive shift); structural mirror.
 *   - src/internal/mpfr/round_raw.ts — substrate.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — ternary direction;
 *     signed zero observable.
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

/**
 * Largest unsigned 64-bit integer: `2^64 - 1`. Matches `ULONG_MAX` on
 * 64-bit Linux (CLAUDE.md Rule 12 platforms).
 *
 * Ref: mpfr/src/div_2ui.c L93 — `n` declared as `unsigned long`.
 */
const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

function validateArgs(
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < 0n || n > ULONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of uint64 range [0, ${ULONG_MAX_VAL}], got ${n}`,
    );
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
 * Compute `x / 2^n` at precision `prec` rounded per `rnd`.
 *
 * @mpfrName mpfr_div_2ui
 *
 * @param x     the input MPFR.
 * @param n     the non-negative power of 2 to divide by, as a bigint
 *              in `[0, 2^64 - 1]`.
 * @param prec  output precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on bad `n`, bad `prec`; `EROUND` on bad
 *                    rounding mode.
 *
 * @example
 *   const a = mpfr_set_si(12n, 53n, 'RNDN').value;
 *   mpfr_div_2ui(a, 2n, 53n, 'RNDN');   // value = 3, ternary 0
 *   mpfr_div_2ui(a, 0n, 53n, 'RNDN');   // value = 12, ternary 0 (no-op)
 */
export function mpfr_div_2ui(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Ref: mpfr/src/div_2ui.c L24-L68 — full function body.
  validateArgs(n, prec, rnd);
  validate(x);

  // --- Singulars -----------------------------------------------------------
  // Mirrors the C `if (n == 0 || MPFR_IS_SINGULAR(x)) return mpfr_set(y, x, rnd_mode);`
  // For n=0 on a normal value, the mantissa refit + exp - 0 produces the
  // correct result naturally via the normal path below, so we only need
  // to special-case true singulars here.
  //
  // Ref: mpfr/src/div_2ui.c L36 — `MPFR_UNLIKELY(n == 0 || MPFR_IS_SINGULAR(x))`.
  switch (x.kind) {
    case 'nan':
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      return {
        value: x.sign === 1 ? posInf(prec) : negInf(prec),
        ternary: 0,
      };
    case 'zero':
      return {
        value: x.sign === 1 ? posZero(prec) : negZero(prec),
        ternary: 0,
      };
    case 'normal':
      break;
  }

  // --- Normal: refit mantissa to prec, then shift exp by -n ----------------
  //
  // Ref: mpfr/src/div_2ui.c L40-L46 — MPFR_SETRAW refits the mantissa
  // (which is exactly what roundMantissa does here), then
  // MPFR_SET_EXP(y, exp - n) shifts the exponent.
  //
  // We refit first so that a carry-up rounding that bumps the exponent
  // by +1 is captured in postExp before we subtract n.
  let postExp: bigint;
  let postMant: bigint;
  let ternary: -1 | 0 | 1;

  if (prec >= x.prec) {
    // Widening or same precision: pad mantissa left with zeros, exact.
    postMant = x.mant << (prec - x.prec);
    postExp = x.exp;
    ternary = 0;
  } else {
    // Narrowing: round to prec bits; may bump exp by +1 on carry-up.
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

  // Ref: mpfr/src/div_2ui.c L54 — `MPFR_SET_EXP(y, exp - (mpfr_exp_t)n)`.
  // No underflow check: TS schema's exp is an unbounded bigint
  // (see §divergence_from_c above).
  const value: MPFR = {
    kind: 'normal',
    sign: x.sign,
    prec,
    exp: postExp - n,
    mant: postMant,
  };
  return { value, ternary };
}
