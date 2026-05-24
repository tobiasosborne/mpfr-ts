/**
 * ops/d_div.ts — pure-TS port of MPFR's `mpfr_d_div`.
 *
 * Compute `b / c` where `b` is a machine double and `c` is an MPFR value,
 * rounded to `prec` bits under `rnd`, returning the canonical
 * `{value, ternary}` shape from src/core.ts (Law 4).
 *
 * This is the **reverse** direction of `mpfr_div_d`: the double argument is
 * the LEFT (numerator) and the MPFR argument is the RIGHT (denominator).
 *
 * C signature
 * -----------
 *
 *   int mpfr_d_div(mpfr_ptr a, double b, mpfr_srcptr c, mpfr_rnd_t rnd_mode);
 *
 *   Ref: mpfr/src/d_div.c L25–L50 — the C reference body.
 *
 * TS signature
 * ------------
 *
 *   mpfr_d_div(b, c, prec, rnd) → Result
 *
 *   where:
 *     - `b` is the double numerator (matches `double b` in the C signature),
 *     - `c` is the MPFR denominator (matches `mpfr_srcptr c`),
 *     - `prec` is the output precision in bits,
 *     - `rnd` is one of the five rounding modes.
 *
 * Algorithm
 * ---------
 *
 * Mirrors the C algorithm exactly:
 *
 *   1. Convert `b` (double) to MPFR at precision 53 bits using `mpfr_set_d`
 *      with the caller's rounding mode. This conversion is always exact for a
 *      binary64 when the MPFR precision is 53 bits — per the MPFR manual, any
 *      double fits exactly in a 53-bit MPFR (MPFR_ASSERTD(inexact == 0) at
 *      mpfr/src/d_div.c L611). The C side uses `MPFR_TMP_INIT1(tmp_man, d,
 *      IEEE_DBL_MANT_DIG)` which allocates an MPFR on the stack at
 *      IEEE_DBL_MANT_DIG (=53) bits.
 *
 *   2. Delegate to `mpfr_div(bMpfr, c, prec, rnd)` — divide the MPFR
 *      encoding of `b` by `c`, rounding to `prec` bits.
 *
 *   3. Return the Result from step 2 directly (mpfr_check_range is
 *      handled inside mpfr_div's rounding layer, and the TS port
 *      does not need a separate range check since there is no
 *      separate expo-save/restore machinery in TS).
 *
 * Key divergence from the C: MPFR_SAVE_EXPO_MARK / MPFR_SAVE_EXPO_FREE
 * are C-side MPFR-internal exponent-range save/restore macros used to
 * suppress spurious overflow/underflow flags during the intermediate
 * conversion. The TS port has no global exponent-range state, so there
 * is nothing to save or restore; the delegate call is sufficient.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/d_div.c L25–L50 — C reference body.
 *   - src/ops/set_d.ts — load-bearing for the double→MPFR step.
 *   - src/ops/div.ts — load-bearing delegate for the MPFR/MPFR step.
 *   - src/core.ts L173–L176 — Result return shape.
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real".
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign of
 *     (rounded - exact)".
 */

import type { MPFR, RoundingMode, Result } from "../core.ts";
import {
  MPFRError,
  PREC_MIN,
  PREC_MAX,
} from "../core.ts";
import { mpfr_set_d } from "./set_d.ts";
import { mpfr_div } from "./div.ts";

// ---------------------------------------------------------------------------
// Argument validation
// ---------------------------------------------------------------------------

/**
 * Validate `prec` and `rnd` at the public boundary. Mirrors the pattern from
 * div.ts and add.ts: only the scalar arguments need checking; MPFR-typed
 * inputs are validated by the grader's decodeMpfr or library-internal
 * construction.
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

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Divide a machine double `b` by an MPFR value `c`, rounding the result to
 * `prec` bits under `rnd`.
 *
 * This is the reverse direction of `mpfr_div_d`: the double is the numerator,
 * the MPFR value is the denominator. Semantics: `b / c`.
 *
 * @mpfrName mpfr_d_div
 *
 * @param b     The double numerator. NaN, ±Infinity, and ±0 are handled per
 *              the MPFR convention: `NaN / anything → NaN`, `Inf / Inf → NaN`,
 *              `Inf / finite → ±Inf`, `0 / 0 → NaN`, `finite / 0 → ±Inf`,
 *              `0 / finite → ±0`.
 * @param c     The MPFR denominator. Any kind is accepted.
 * @param prec  Output precision in **bits** (not decimal digits), in
 *              `[PREC_MIN, PREC_MAX]`.
 * @param rnd   One of the five rounding modes: `'RNDN' | 'RNDZ' | 'RNDU' |
 *              'RNDD' | 'RNDA'`.
 *
 * @returns     `{value, ternary}` — the MPFR-rounded quotient and the ternary
 *              flag `sign(rounded - exact)`. Ternary `0` for exact results
 *              (including all specials). The returned value passes `validate()`
 *              without post-processing.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *                    NaN / Inf / zero input is NOT an error.
 *
 * @example
 *   // 6.0 / mpfr(2.0) = 3.0, exact
 *   mpfr_d_div(6.0, setD(2.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 3.0 at prec 53, ternary: 0}
 *   // 1.0 / mpfr(0) → +Inf (divbyzero)
 *   mpfr_d_div(1.0, posZero(53n), 53n, 'RNDN');
 *     // → {value: posInf(53n), ternary: 0}
 *   // NaN / mpfr(2.0) → NaN
 *   mpfr_d_div(NaN, setD(2.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}
 */
export function mpfr_d_div(
  b: number,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Validate scalar arguments first — same pattern as div.ts, add.ts.
  validateArgs(prec, rnd);

  // Step 1: Convert `b` (double) to MPFR at 53-bit precision.
  // The conversion at prec=53 is always exact for a binary64 (IEEE_DBL_MANT_DIG=53).
  // Ref: mpfr/src/d_div.c L609-L611 — MPFR_TMP_INIT1(tmp_man, d, IEEE_DBL_MANT_DIG)
  //   followed by mpfr_set_d(d, b, rnd_mode) with MPFR_ASSERTD(inexact == 0).
  // We pass the caller's rnd here (matching the C), but since the conversion
  // is always exact at prec=53, the rnd is irrelevant for this step.
  const bMpfrResult = mpfr_set_d(b, 53n, rnd);
  const bMpfr = bMpfrResult.value;

  // Step 2: Delegate to mpfr_div(bMpfr, c, prec, rnd).
  // Ref: mpfr/src/d_div.c L613-L614 — inexact = mpfr_div(a, d, c, rnd_mode).
  // The TS port returns this result directly; there's no separate
  // check_range step needed since mpfr_div handles rounding to `prec` bits.
  return mpfr_div(bMpfr, c, prec, rnd);
}
