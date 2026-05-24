/**
 * ops/mul_2ui.ts — pure-TS port of MPFR's `mpfr_mul_2ui`.
 *
 * Multiply an {@link MPFR} value by `2^n` where `n` is a non-negative
 * integer (unsigned long in C). The mantissa is unchanged; only the
 * binary exponent shifts by `+n`. If the caller-supplied `prec` differs
 * from `x.prec` the mantissa is re-fitted via the shared rounding
 * substrate.
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul_2ui(mpfr_ptr y, mpfr_srcptr x, unsigned long int n, mpfr_rnd_t rnd_mode)
 *
 *   Ref: mpfr/src/mul_2ui.c L24–L50. Algorithm:
 *
 *     1. inexact = (y != x) ? mpfr_set(y, x, rnd_mode) : 0;
 *        (i.e. refit mantissa to rop's precision first)
 *     2. If MPFR_IS_PURE_FP(y): check exp + n > emax → overflow.
 *        Otherwise: MPFR_SET_EXP(y, exp + n).
 *     3. Return inexact.
 *
 *   The TS port omits the C-side emax/emin range check: the locked
 *   schema (src/core.ts) places no upper or lower bound on the exponent
 *   field of a `normal` MPFR — `exp` is an arbitrary-precision bigint.
 *   This matches the analogous divergence in mpfr_mul_2si.ts.
 *
 *   Ref: src/ops/mul_2si.ts §divergence_from_c — same policy.
 *
 * TS signature
 * ------------
 *
 *   mpfr_mul_2ui(x: MPFR, n: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - `n` is a `bigint >= 0n` (unsigned). The C function takes
 *     `unsigned long`; the TS port accepts any non-negative bigint.
 *   - `prec` is positional (no `rop`); the value is refitted to it.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `n` (must be bigint >= 0n), `prec`, `rnd`, structural
 *      validity of `x`.
 *   2. NaN: return `{value: NAN_VALUE, ternary: 0}`. NaN ignores `n`,
 *      `prec`, `rnd`. (NAN_VALUE has `prec=0n` per the schema's NaN
 *      convention.)
 *   3. ±Inf / ±0: return the singular value at the caller-supplied
 *      precision with sign preserved. Ternary 0 — there's nothing to
 *      round.
 *   4. Normal: refit the mantissa to `prec` first (mirrors MPFR_SETRAW
 *      from mpfr/src/mul_2ui.c), then add `n` to the post-refit
 *      exponent.
 *      - If `prec >= x.prec`: pad the mantissa with `prec - x.prec`
 *        zero bits on the right. Exp post-refit equals x.exp. Ternary 0
 *        (exact widening).
 *      - If `prec < x.prec`: delegate to `roundMantissa` for the
 *        rounding step. Substrate may bump the post-refit exponent on
 *        carry-out. Ternary follows the substrate.
 *      Final exp = post-refit exp + n. Mantissa unchanged from refit.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/mul_2ui.c L24–L50 — C reference.
 *   - src/ops/mul_2si.ts — sibling signed port; this is the unsigned
 *     specialisation (n >= 0n).
 *   - src/internal/mpfr/round_raw.ts — substrate.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — ternary direction,
 *     rounding-mode count is FIVE, signed zero observable.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
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
} from "../core.ts";
import { roundMantissa } from "../internal/mpfr/round_raw.ts";

/**
 * Validate boundary scalars.
 *
 * `n` must be a non-negative bigint (mirrors `unsigned long` in C).
 * The schema places no limit on the resulting exponent.
 */
function validateArgs(
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < 0n) {
    throw new MPFRError('EPREC', `n must be >= 0n (unsigned), got ${n}`);
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
 * Compute `x * 2^n` at precision `prec` rounded per `rnd`.
 *
 * @mpfrName mpfr_mul_2ui
 *
 * @param x     the input MPFR. Must pass {@link validate}.
 * @param n     the non-negative power of 2 to multiply by, as a bigint.
 *              C takes `unsigned long`; TS accepts any non-negative bigint.
 * @param prec  output precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on bad prec, negative `n`, or non-bigint
 *                     arguments; `EROUND` on bad rounding mode.
 *
 * @example
 *   const a = mpfr_set_si(3n, 53n, 'RNDN').value;
 *   mpfr_mul_2ui(a, 2n, 53n, 'RNDN');   // value = 12, ternary 0
 *   mpfr_mul_2ui(a, 0n, 53n, 'RNDN');   // value = 3, ternary 0
 */
export function mpfr_mul_2ui(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(n, prec, rnd);
  validate(x);

  // --- Singulars -----------------------------------------------------------
  // Ref: mpfr/src/mul_2ui.c L24-L50.
  // C: inexact = (y != x) ? mpfr_set(y, x, rnd) : 0; then checks
  // MPFR_IS_PURE_FP. For NaN / ±Inf / ±0, mpfr_set copies through;
  // there is no mantissa to round and the exponent shift is a no-op
  // for non-normal values. Ternary 0 — singulars are exact.
  switch (x.kind) {
    case 'nan':
      // NaN ignores n, prec, rnd. Returns canonical NAN_VALUE
      // (prec=0n per the schema's NaN convention).
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      return { value: x.sign === 1 ? posInf(prec) : negInf(prec), ternary: 0 };
    case 'zero':
      // Signed zero is observable: +0 → +0, -0 → -0 (CLAUDE.md §hallucination-risk).
      return {
        value: x.sign === 1 ? posZero(prec) : negZero(prec),
        ternary: 0,
      };
    case 'normal':
      break;
  }

  // --- Normal: refit mantissa to prec, then shift exp by n -----------------
  //
  // Ref: mpfr/src/mul_2ui.c L24-L50 — C does mpfr_set(y, x, rnd) first
  // (refitting the mantissa to rop's precision), then sets
  // MPFR_SET_EXP(y, exp + n).
  //
  // We follow the same order: refit mantissa first so that a carry-out
  // from rounding-up reaches the exponent before the +n shift is applied.
  let postExp: bigint;
  let postMant: bigint;
  let ternary: -1 | 0 | 1;

  if (prec >= x.prec) {
    // Lossless padding: shift the mantissa left to widen MSB-aligned to
    // prec bits. Exp unchanged; ternary 0 (exact widening).
    postMant = x.mant << (prec - x.prec);
    postExp = x.exp;
    ternary = 0;
  } else {
    // Lossy rounding to a narrower precision. The substrate handles
    // carry-out by renormalising and bumping the exponent.
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

  // Apply the 2^n shift to the post-refit exponent.
  // No emax clamp — see spec.json's divergence_from_c; the TS schema
  // allows unbounded exponents on normal values.
  // Ref: mpfr/src/mul_2ui.c L48 — MPFR_SET_EXP(y, exp + (mpfr_exp_t) n).
  const value: MPFR = {
    kind: 'normal',
    sign: x.sign,
    prec,
    exp: postExp + n,
    mant: postMant,
  };
  return { value, ternary };
}
