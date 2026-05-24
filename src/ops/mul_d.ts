/**
 * ops/mul_d.ts — pure-TS port of MPFR's `mpfr_mul_d`.
 *
 * Multiply an {@link MPFR} value `b` by a machine double `c`, rounded to
 * `prec` bits under rounding mode `rnd`, returning the canonical
 * `{value, ternary}` shape from src/core.ts (Law 4).
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul_d(mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode);
 *
 *   Ref: mpfr/src/mul_d.c L25–L50 — the C reference body.
 *
 * TS signature
 * ------------
 *
 *   mpfr_mul_d(b: MPFR, c: number, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 * The C reference is a trivial two-step wrapper:
 *
 *   1. Stack-allocate a temporary mpfr_t at prec = IEEE_DBL_MANT_DIG (53),
 *      then call `mpfr_set_d(d, c, rnd_mode)` to convert the double exactly.
 *      The conversion is exact at 53 bits (a double carries exactly 53 bits
 *      of mantissa information), so the result is MPFR_ASSERTD to be 0.
 *
 *   2. Delegate to `mpfr_mul(a, b, d, rnd_mode)` for the actual product and
 *      rounding.
 *
 * The TS port mirrors the same two steps, using the already-shipped
 * `mpfr_set_d` (src/ops/set_d.ts) for step 1 and `mpfr_mul`
 * (src/ops/mul.ts) for step 2.
 *
 * Ref: mpfr/src/mul_d.c L25–L50:
 *
 *   MPFR_TMP_INIT1(tmp_man, d, IEEE_DBL_MANT_DIG);
 *   inexact = mpfr_set_d (d, c, rnd_mode);
 *   MPFR_ASSERTD (inexact == 0);
 *   MPFR_CLEAR_FLAGS ();
 *   inexact = mpfr_mul (a, b, d, rnd_mode);
 *   MPFR_SAVE_EXPO_UPDATE_FLAGS (expo, __gmpfr_flags);
 *   MPFR_SAVE_EXPO_FREE (expo);
 *   return mpfr_check_range (a, inexact, rnd_mode);
 *
 * Hallucination-risk notes
 * ------------------------
 *
 * - Signed zero: the sign of the product is the product-of-signs of b.sign
 *   and the sign embedded in `c`. Both mpfr_set_d and mpfr_mul handle this
 *   correctly per their own contracts; we do not override it here.
 *   (CLAUDE.md "Signed zero is real".)
 *
 * - Ternary: the ternary flag is the sign of (rounded - exact) at the
 *   result precision. mpfr_mul computes this correctly; we propagate it
 *   verbatim. (CLAUDE.md "Ternary flag is the sign of (rounded - exact)".)
 *
 * - The set_d at prec=53n is always exact (mpfr_set_d for a double at
 *   prec=53 produces ternary=0 for all finite non-NaN inputs — verified
 *   by the C-side MPFR_ASSERTD). NaN/Inf/zero specials at prec=53 also
 *   return ternary=0. We ignore the ternary from set_d.
 *
 * - Class is 'misc' — this is a surface wrapper, not a substrate helper.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/mul_d.c L25–L50 — C reference body.
 *   - src/ops/set_d.ts — double-to-MPFR conversion at prec=53.
 *   - src/ops/mul.ts   — the MPFR multiplication that does the real work.
 *   - src/core.ts      — locked schema (MPFR, RoundingMode, Result, Ternary).
 *   - CLAUDE.md Law 4  — every port imports from src/core.ts.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import { MPFRError, PREC_MIN, PREC_MAX } from "../core.ts";
import { mpfr_set_d } from "./set_d.ts";
import { mpfr_mul } from "./mul.ts";

/**
 * Validate the public-boundary scalar arguments. Throws `MPFRError` on
 * bad `prec` or `rnd`. The `b` operand is assumed pre-validated (internal
 * callers and the runner both produce validated MPFR values). The double
 * `c` accepts all IEEE 754 values including NaN, ±Inf, and ±0.
 */
function validateArgs(prec: bigint, rnd: RoundingMode): void {
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
 * Multiply MPFR value `b` by machine double `c`, rounding the result to
 * `prec` bits under rounding mode `rnd`.
 *
 * @mpfrName mpfr_mul_d
 *
 * @param b     the MPFR operand. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param c     the machine double operand. All IEEE 754 values are accepted:
 *              NaN, ±Infinity, ±0, normals, and subnormals.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}` as per the locked schema. The value passes
 *              `validate()` without post-processing. The ternary is the sign
 *              of `(rounded - exact)` at the target precision.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *                    NaN / Inf inputs (in either `b` or `c`) are NOT errors.
 *
 * @example
 *   // 2.0 * 3.0 at prec=53, RNDN → 6.0, exact
 *   mpfr_mul_d(mpfr_set_d(2.0, 53n, 'RNDN').value, 3.0, 53n, 'RNDN');
 *     // → { value: 6.0 at prec 53, ternary: 0 }
 *
 *   // Inf * 2.0 → +Inf, exact
 *   mpfr_mul_d(posInf(53n), 2.0, 53n, 'RNDN');
 *     // → { value: posInf(53n), ternary: 0 }
 *
 *   // 0.0 * Infinity → NaN (indeterminate)
 *   mpfr_mul_d(posZero(53n), Infinity, 53n, 'RNDN');
 *     // → { value: NAN_VALUE, ternary: 0 }
 */
export function mpfr_mul_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Validate scalar arguments at the public boundary. Throws on bad prec/rnd.
  validateArgs(prec, rnd);

  // Step 1: Convert the double `c` to an MPFR value at prec=53 bits.
  //
  // Ref: mpfr/src/mul_d.c L609–L610:
  //   MPFR_TMP_INIT1(tmp_man, d, IEEE_DBL_MANT_DIG);
  //   inexact = mpfr_set_d(d, c, rnd_mode);
  //   MPFR_ASSERTD(inexact == 0);
  //
  // The conversion is exact at 53 bits for all finite doubles (a double
  // carries exactly 53 mantissa bits). NaN / ±Inf / ±0 are also exact
  // (ternary=0). The rounding mode passed to set_d is the same as the
  // final rnd — C passes rnd_mode here too, but it doesn't matter
  // because the conversion is always exact. We use the same rnd for
  // consistency with the C reference.
  const dMpfr: MPFR = mpfr_set_d(c, 53n, rnd).value;

  // Step 2: Multiply b by the MPFR representation of c, rounding to prec.
  //
  // Ref: mpfr/src/mul_d.c L614:
  //   inexact = mpfr_mul(a, b, d, rnd_mode);
  //
  // mpfr_mul handles all special combinations (NaN, Inf, zero) per its
  // own contract (src/ops/mul.ts). The ternary from this call is the
  // final ternary flag for mpfr_mul_d.
  return mpfr_mul(b, dMpfr, prec, rnd);
}
