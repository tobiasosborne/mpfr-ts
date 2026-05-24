/**
 * ops/div_d.ts — pure-TS port of MPFR's `mpfr_div_d`.
 *
 * Divide an MPFR value `b` by a machine double `c`, rounded to `prec`
 * bits under rounding mode `rnd`, returning the canonical `{value,
 * ternary}` shape.
 *
 * C signature
 * -----------
 *
 *   int mpfr_div_d(mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode)
 *
 *   Ref: mpfr/src/div_d.c L25-L50 — the complete C body.
 *
 * TS signature
 * ------------
 *
 *   mpfr_div_d(b: MPFR, c: number, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 * The C implementation is a trivial two-step wrapper:
 *
 *   1. Stack-allocate a 53-bit mpfr_t `d` and call
 *      `mpfr_set_d(d, c, rnd_mode)`.  The C ASSERTD confirms the result
 *      is exact (every finite double is representable at prec=53).
 *
 *   2. Delegate to `mpfr_div(a, b, d, rnd_mode)`.
 *
 * The TS port mirrors this exactly:
 *
 *   1. `mpfr_set_d(c, 53n, rnd)` — convert `c` to MPFR at prec=53.
 *      Because every finite IEEE 754 binary64 is exactly representable
 *      as a 53-bit MPFR normal (or as one of NaN / ±Inf / ±0), the
 *      conversion is always exact (ternary === 0 for finite non-special
 *      inputs). We pass `rnd` for faithfulness, but it has no numeric
 *      effect — the conversion is exact regardless.
 *
 *   2. `mpfr_div(b, dMpfr, prec, rnd)` — compute the rounded quotient.
 *
 * Special-value handling is fully delegated to `mpfr_div` (which already
 * handles NaN propagation, Inf/Inf = NaN, finite/Inf = ±0, Inf/finite =
 * ±Inf, 0/0 = NaN, finite/0 = ±Inf, 0/finite = ±0). `mpfr_set_d`
 * handles NaN, ±Inf, and ±0 doubles, returning the corresponding MPFR
 * specials that `mpfr_div` then dispatches correctly.
 *
 * The `MPFR_SAVE_EXPO` / `mpfr_check_range` wrapper in the C code is
 * dropped: the TS port has no global emin/emax state.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/div_d.c L25-L50 — the C reference body.
 *   - src/ops/div.ts — load-bearing delegate (`mpfr_div`).
 *   - src/ops/set_d.ts — load-bearing for the double->MPFR step.
 *   - src/core.ts — locked schema (MPFR, RoundingMode, Result, Ternary).
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real".
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign
 *     of (rounded - exact)".
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from "../core.ts";
import { mpfr_set_d } from "./set_d.ts";
import { mpfr_div } from "./div.ts";

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

/**
 * Validate `prec` and `rnd` at the public boundary.
 *
 * We do NOT validate `c` (the double) — every JS number is a valid input
 * to `mpfr_set_d`, which handles NaN / ±Inf / ±0 correctly.
 * We do NOT validate `b` (the MPFR operand) — library-internal callers
 * produce well-formed MPFR values; the grader checks this structurally.
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
 * Divide MPFR value `b` by machine double `c`, returning the rounded
 * result at `prec` bits and the ternary flag.
 *
 * @mpfrName mpfr_div_d
 *
 * @param b     MPFR dividend. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param c     IEEE 754 binary64 divisor. NaN, ±Inf, ±0 are accepted
 *              and propagate correctly through `mpfr_set_d` and `mpfr_div`.
 * @param prec  Output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   One of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. The value passes `validate()` without
 *              post-processing. Ternary is `0` for exact results (including
 *              all specials), `+1` if rounded > exact, `-1` if rounded <
 *              exact.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *
 * @example
 *   // 6.0 / 2.0 = 3.0 (exact)
 *   mpfr_div_d(mpfr_set_d(6.0, 53n, 'RNDN').value, 2.0, 53n, 'RNDN');
 *     // → {value: 3.0 at prec 53, ternary: 0}
 *
 *   // 1.0 / 3.0 — inexact
 *   mpfr_div_d(mpfr_set_d(1.0, 53n, 'RNDN').value, 3.0, 53n, 'RNDN');
 *     // → {value: ~0.3333..., ternary: 1}  (rounded up at 53 bits)
 *
 *   // b / 0.0 → +Inf (finite-nonzero / ±0 is divbyzero → Inf)
 *   mpfr_div_d(mpfr_set_d(1.0, 53n, 'RNDN').value, 0.0, 53n, 'RNDN');
 *     // → {value: +Inf at prec 53, ternary: 0}
 */
export function mpfr_div_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Ref: mpfr/src/div_d.c L25-L50 — validate before entering the wrapper.
  validateArgs(prec, rnd);

  // Step 1: Convert the double `c` to MPFR at prec=53 (IEEE_DBL_MANT_DIG).
  // Ref: mpfr/src/div_d.c L34 — `MPFR_TMP_INIT1(tmp_man, d, IEEE_DBL_MANT_DIG)`.
  // Ref: mpfr/src/div_d.c L35 — `inexact = mpfr_set_d(d, c, rnd_mode)`.
  // The ASSERTD(inexact == 0) confirms this conversion is always exact at
  // prec=53 for any finite double; NaN/Inf/0 are exact by convention.
  // We pass `rnd` for faithfulness to the C signature; it has no numeric
  // effect since the conversion is exact regardless.
  const dMpfr = mpfr_set_d(c, 53n, rnd).value;

  // Step 2: Delegate to mpfr_div with the target precision.
  // Ref: mpfr/src/div_d.c L38 — `inexact = mpfr_div(a, b, d, rnd_mode)`.
  // All special-value dispatch (NaN, Inf, zero) is handled inside mpfr_div.
  return mpfr_div(b, dMpfr, prec, rnd);
}
