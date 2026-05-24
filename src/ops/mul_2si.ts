/**
 * ops/mul_2si.ts — pure-TS port of MPFR's `mpfr_mul_2si`.
 *
 * Multiply an {@link MPFR} value by `2^e`. The mantissa is unchanged
 * (and so is the represented sign and kind); only the binary exponent
 * shifts by `e`. If the caller-supplied `prec` differs from `x.prec`
 * the mantissa is re-fitted via the shared rounding substrate.
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul_2si(mpfr_t rop, mpfr_srcptr op, long e, mpfr_rnd_t rnd);
 *
 *   Ref: mpfr/src/mul_2si.c L24–L58. Algorithm:
 *
 *     1. If op is a singular value (NaN, ±Inf, ±0): defer to mpfr_set
 *        (i.e. copy through with the rounded mantissa if precs differ;
 *        for singular kinds there's no mantissa to round, so this just
 *        propagates the kind/sign and adopts the target precision).
 *     2. Otherwise: take exp = MPFR_GET_EXP(x), refit the mantissa to
 *        the target precision via MPFR_SETRAW (which is mpfr_round_raw
 *        on the prec-shrinking path; a copy + pad on the widening path),
 *        check exp + n for overflow/underflow against the active emax /
 *        emin, then set the new exponent to exp + n.
 *
 *   The TS port omits the C-side emax/emin range check: the locked
 *   schema (src/core.ts) places no upper or lower bound on the exponent
 *   field of a `normal` MPFR — `exp` is an arbitrary-precision bigint
 *   in this surface, mirroring `e` itself. A caller wanting saturating
 *   semantics can layer a range check on top; the substrate honours
 *   whatever exponent the math produces. This matches our treatment in
 *   neg.ts / abs.ts (also no emax/emin) and is documented in
 *   spec.json's divergence_from_c.
 *
 * TS signature
 * ------------
 *
 *   mpfr_mul_2si(x: MPFR, e: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - `e` is a `bigint` (matching the way set_si takes its integer arg)
 *     so callers can express the full int64 range and beyond. The C
 *     function takes `long`; on 64-bit Linux that's int64 — the TS port
 *     accepts the full bigint range, since there's no hardware-int
 *     limitation in JS.
 *   - `prec` is positional (no `rop`); the value is refitted to it.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `prec`, `rnd`, structural validity of `x`.
 *   2. NaN: return `{value: NAN_VALUE, ternary: 0}`. NaN ignores `e`,
 *      ignores `prec`, ignores `rnd`. (NAN_VALUE has `prec=0n` per the
 *      schema's NaN convention.)
 *   3. ±Inf / ±0: return the singular value at the caller-supplied
 *      precision with sign preserved. Ternary 0 — there's nothing to
 *      round, and the C `mpfr_set` returns 0 for singulars too.
 *   4. Normal: refit the mantissa to `prec` first (this matches
 *      MPFR_SETRAW from mpfr/src/mul_2si.c L39), then add `e` to the
 *      post-refit exponent.
 *      - If `prec >= x.prec`: pad the mantissa with `prec - x.prec`
 *        zero bits on the right. Exp post-refit equals x.exp. Ternary 0
 *        (exact widening).
 *      - If `prec < x.prec`: delegate to `roundMantissa` for the
 *        rounding step. Substrate may bump the post-refit exponent on
 *        carry-out. Ternary follows the substrate.
 *      Final exp = post-refit exp + e. Mantissa unchanged from refit.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/mul_2si.c L24–L58 — C reference.
 *   - src/internal/mpfr/round_raw.ts — substrate.
 *   - src/ops/set_si.ts — analogous "MSB-aligned + pad or round" shape.
 *   - src/ops/neg.ts — same approach to prec-refit on a value-preserving
 *     transform.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — ternary direction,
 *     rounding-mode count is FIVE, signed zero observable.
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
 * Validate the boundary scalars. Same shape as set_si / set_d. The `e`
 * argument is unbounded (an arbitrary bigint) — we only check it is a
 * bigint at all. The schema places no limit on the result's exponent.
 */
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
 * Compute `x * 2^e` at precision `prec` rounded per `rnd`.
 *
 * @mpfrName mpfr_mul_2si
 *
 * @param x     the input MPFR. Must pass {@link validate}.
 * @param e     the (possibly negative) power of 2 to multiply by, as a
 *              bigint. C takes `long`; TS accepts any bigint.
 * @param prec  output precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on bad prec or non-bigint `e`;
 *                    `EROUND` on bad rounding mode.
 *
 * @example
 *   const a = mpfr_set_si(3n, 53n, 'RNDN').value;
 *   mpfr_mul_2si(a, 2n, 53n, 'RNDN');   // value = 12, ternary 0
 *   mpfr_mul_2si(a, -2n, 53n, 'RNDN');  // value = 0.75, ternary 0
 */
export function mpfr_mul_2si(
  x: MPFR,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(e, prec, rnd);
  validate(x);

  // --- Singulars -----------------------------------------------------------
  // C: MPFR_IS_SINGULAR(x) → mpfr_set(y, x, rnd). For NaN / ±Inf / ±0 the
  // "multiply by 2^e" operation is a no-op on the represented value; we
  // adopt the caller-supplied precision. Ternary is 0 — there's no
  // mantissa to round, and the C mpfr_set returns 0 for singulars.
  switch (x.kind) {
    case 'nan':
      // NaN ignores everything: prec, e, rnd. Returns the canonical
      // NAN_VALUE (prec=0n per the schema's NaN convention).
      return { value: NAN_VALUE, ternary: 0 };
    case 'inf':
      return { value: x.sign === 1 ? posInf(prec) : negInf(prec), ternary: 0 };
    case 'zero':
      // Signed zero observable: +0 → +0, -0 → -0 even under scaling by 2^e.
      return {
        value: x.sign === 1 ? posZero(prec) : negZero(prec),
        ternary: 0,
      };
    case 'normal':
      break;
  }

  // --- Normal: refit mantissa to prec, then shift exp by e ------------------
  //
  // The C function does MPFR_SETRAW first (which adjusts the mantissa to
  // the target precision) and THEN bumps the exponent by n. We do the
  // same here so the carry-out from a rounding-up refit reaches the
  // exponent before the +e shift.
  let postExp: bigint;
  let postMant: bigint;
  let ternary: -1 | 0 | 1;

  if (prec >= x.prec) {
    // Lossless padding: shift the mantissa left to widen MSB-aligned to
    // prec bits. Exp unchanged at this step; ternary 0.
    postMant = x.mant << (prec - x.prec);
    postExp = x.exp;
    ternary = 0;
  } else {
    // Lossy rounding to a narrower precision. Substrate handles
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

  // Apply the 2^e shift to the post-refit exponent. No emax/emin clamp
  // — see spec.json's divergence_from_c.
  const value: MPFR = {
    kind: 'normal',
    sign: x.sign,
    prec,
    exp: postExp + e,
    mant: postMant,
  };
  return { value, ternary };
}
