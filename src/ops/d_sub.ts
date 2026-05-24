/**
 * port.ts — eval port of MPFR's `mpfr_d_sub` for grading.
 *
 * Compute `b - c` where `b` is a machine double and `c` is an MPFR value,
 * rounded to `prec` bits under `rnd`. This is the reverse-operand sibling
 * of `mpfr_sub_d` — the double is the LEFT (minuend) operand.
 *
 * C signature
 * -----------
 *
 *   int mpfr_d_sub(mpfr_ptr a, double b, mpfr_srcptr c, mpfr_rnd_t rnd_mode);
 *
 *   Body (mpfr/src/d_sub.c L25-L50):
 *
 *     MPFR_TMP_INIT1(tmp_man, d, IEEE_DBL_MANT_DIG);  // 53-bit temp
 *     inexact = mpfr_set_d(d, b, rnd_mode);
 *     MPFR_ASSERTD(inexact == 0);                      // always exact
 *     inexact = mpfr_sub(a, d, c, rnd_mode);           // d - c, NOT c - d
 *     return mpfr_check_range(a, inexact, rnd_mode);
 *
 *   Ref: mpfr/src/d_sub.c L25-L50 — the C reference body.
 *
 * TS Algorithm
 * ------------
 *
 * Two-step composition, mirroring the C:
 *
 *   1. Convert `b` (double) → MPFR at prec=53 via `mpfr_set_d`. The
 *      conversion is always exact (IEEE_DBL_MANT_DIG == 53; no rounding
 *      loss). The C ASSERTD(inexact == 0) confirms this.
 *
 *   2. Delegate to `mpfr_sub(bMpfr, c, prec, rnd)`. Operand order is
 *      critical: the double (now `bMpfr`) is the MINUEND (first arg),
 *      and `c` is the SUBTRAHEND (second arg). This computes `b - c`,
 *      NOT `c - b`.
 *
 * Note on rounding mode for step 1
 * ---------------------------------
 *
 * The C code passes `rnd_mode` to `mpfr_set_d`, but the subsequent
 * ASSERTD(inexact == 0) proves the conversion is always exact — the
 * rounding mode is irrelevant for the set_d step. In the TS port we
 * pass 'RNDN' for step 1 (any mode works since no rounding occurs),
 * which makes the intent clearer. The sole rounding that matters is
 * the final mpfr_sub step.
 *
 * TS divergences from C
 * ----------------------
 *
 *   1. No check_range tail (schema has no configurable emin/emax).
 *   2. The TMP_INIT1 stack-allocation becomes a transient MPFR value
 *      at prec=53n — no allocation difference in TS (GC handles it).
 *   3. Explicit prec parameter on the TS side; C reads prec from `rop`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/d_sub.c L25-L50 — C reference body.
 *   - src/ops/set_d.ts — double-to-MPFR conversion (step 1 delegate).
 *   - src/ops/sub.ts — MPFR subtraction (step 2 delegate).
 *   - src/core.ts L173-L176 — Result return shape.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN != NaN" — handled by
 *     the sub delegate.
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *     handled by set_d (Object.is -0 check) and sub delegate.
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign
 *     of (rounded - exact), not 0/1" — ternary flows from sub delegate.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from "../core.ts";
import { mpfr_set_d } from "./set_d.ts";
import { mpfr_sub } from "./sub.ts";

/**
 * Validate `prec` and `rnd` at the public boundary. Throws `MPFRError`
 * on bad input (`EPREC` for precision, `EROUND` for rounding mode).
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
 * Subtract an MPFR value `c` from a machine double `b`, returning the
 * result at `prec` bits, rounded per `rnd`.
 *
 * @mpfrName mpfr_d_sub
 *
 * @param b     the double (IEEE 754 binary64) — the LEFT operand (minuend).
 *              NaN, ±Infinity, and ±0 are all handled via set_d.
 * @param c     the MPFR subtrahend — the RIGHT operand. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}` where `value = b - c` rounded to `prec`
 *              bits under `rnd`, and `ternary` is the sign of
 *              `(rounded - exact)`.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf / ±0 inputs are NOT errors.
 *
 * @example
 *   // 3.0 - 1.0 = 2.0
 *   mpfr_d_sub(3.0, setD(1.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 2.0 at prec=53, ternary: 0}
 *
 *   // NaN - anything = NaN
 *   mpfr_d_sub(NaN, setD(1.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}
 */
export function mpfr_d_sub(
  b: number,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Boundary validation: prec and rnd. Any `number` is a valid `b`.
  validateArgs(prec, rnd);

  // Step 1: Convert double b to MPFR at prec=53 (IEEE_DBL_MANT_DIG).
  // The C reference uses MPFR_TMP_INIT1 at 53 bits and asserts inexact==0,
  // meaning the conversion is always exact regardless of rnd_mode.
  // We use 'RNDN' here to be explicit; any mode gives the same result.
  //
  // Ref: mpfr/src/d_sub.c L30-L31 — MPFR_TMP_INIT1(tmp_man, d, IEEE_DBL_MANT_DIG)
  //   and mpfr_set_d(d, b, rnd_mode) with MPFR_ASSERTD(inexact == 0).
  const bMpfr: MPFR = mpfr_set_d(b, 53n, 'RNDN').value;

  // Step 2: Compute bMpfr - c at the target precision.
  // CRITICAL: bMpfr is the MINUEND (first arg to mpfr_sub), c is the
  // SUBTRAHEND (second arg). This gives b - c, NOT c - b.
  //
  // Ref: mpfr/src/d_sub.c L34 — mpfr_sub(a, d, c, rnd_mode)
  //   where `d` is the converted double (bMpfr here) and `c` is the
  //   MPFR operand. Operand order is load-bearing.
  return mpfr_sub(bMpfr, c, prec, rnd);
}
