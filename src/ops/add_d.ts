/**
 * ops/add_d.ts — pure-TS port of MPFR's `mpfr_add_d`.
 *
 * Add an {@link MPFR} value `b` to a machine double `c` and round to
 * `prec` bits under rounding mode `rnd`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_add_d(mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode);
 *
 *   - mutates `a` in place (precision comes from `a`);
 *   - returns the ternary as the function result.
 *
 *   Ref: mpfr/src/add_d.c L25-L50 — the C reference body.
 *
 * TS signature
 * ------------
 *
 *   mpfr_add_d(b: MPFR, c: number, prec: bigint, rnd: RoundingMode): Result
 *
 *   - takes `prec` as an explicit positional argument (no `rop`);
 *   - returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Algorithm
 * ---------
 *
 * The C reference is a trivial 25-line wrapper (Ref: mpfr/src/add_d.c
 * L25-L50):
 *
 *   1. Stack-allocate an mpfr_t `d` with prec = IEEE_DBL_MANT_DIG (= 53).
 *   2. Call mpfr_set_d(d, c, rnd_mode) — exact for all finite doubles
 *      since 53 bits suffice for any binary64. For NaN / ±Inf, mpfr_set_d
 *      converts them to the corresponding MPFR special.
 *   3. Delegate to mpfr_add(a, b, d, rnd_mode).
 *   4. The ternary and rounded value come entirely from mpfr_add; no
 *      further rounding is applied.
 *
 * The MPFR_ASSERTD(inexact == 0) in the C source confirms that
 * mpfr_set_d at prec=53 is always exact — the ternary from mpfr_set_d
 * is discarded (only the mpfr_add ternary is returned).
 *
 * This TS port mirrors the C body exactly:
 *
 *   const dMpfr = mpfr_set_d(c, 53n, rnd).value;   // exact conversion
 *   return mpfr_add(b, dMpfr, prec, rnd);           // delegate
 *
 * Hallucination-risk callouts
 * ---------------------------
 *
 * - NaN double input: mpfr_set_d(NaN, 53n, rnd) returns NAN_VALUE, which
 *   then propagates through mpfr_add as NaN. The double-NaN path
 *   exercises the CLAUDE.md "NaN != NaN" callout — the golden correctness
 *   check must use a NaN-aware comparison, not `===`.
 *
 * - Signed zero: Object.is(-0.0, -0) is true, and mpfr_set_d preserves
 *   signed zero. A double `-0` becomes negZero(53n) which then
 *   participates in mpfr_add's standard signed-zero arithmetic. Getting
 *   this wrong produces wrong results on ~30% of edge goldens.
 *
 * - Ternary direction: mpfr_add returns sign(rounded - exact), not
 *   sign(exact - rounded). We pass the ternary through from mpfr_add
 *   verbatim — no sign flip.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add_d.c L25-L50 — C reference body.
 *   - src/ops/add.ts — load-bearing delegate after double->MPFR conversion.
 *   - src/ops/set_d.ts — load-bearing for the double->MPFR step.
 *   - src/core.ts — value model (MPFR, RoundingMode, Result, Ternary).
 *   - CLAUDE.md "Hallucination-risk callouts: NaN != NaN".
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real".
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is sign of
 *     (rounded - exact), not 0/1".
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import { MPFRError, PREC_MAX, PREC_MIN } from "../core.ts";
import { mpfr_set_d } from "./set_d.ts";
import { mpfr_add } from "./add.ts";

// IEEE_DBL_MANT_DIG — the mantissa precision of a binary64, in bits.
// MPFR's C side uses this constant (from <float.h>) as the prec for the
// stack-allocated mpfr_t that holds the converted double. It is 53 for
// all IEEE 754 conformant platforms.
// Ref: mpfr/src/add_d.c L43 — MPFR_TMP_INIT1(tmp_man, d, IEEE_DBL_MANT_DIG)
const IEEE_DBL_MANT_DIG: bigint = 53n;

/**
 * Validate the public-boundary arguments. Throws `MPFRError` on bad
 * inputs. Mirrors the validateArgs pattern in add.ts and set_d.ts.
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
 * Add an MPFR value `b` to a machine double `c`, rounding the result to
 * `prec` bits under `rnd`.
 *
 * @mpfrName mpfr_add_d
 *
 * @param b     the MPFR operand. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param c     the IEEE 754 binary64 operand. NaN, ±Infinity, and ±0 are
 *              all handled; signed zero is preserved.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}` from `mpfr_add(b, set_d(c, 53n, rnd), prec,
 *              rnd)`. The value passes `validate()` without post-processing.
 *              Ternary is `0` for exact, `+1` if rounded > exact, `-1` if
 *              rounded < exact.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *
 * @example
 *   mpfr_add_d(setD(1.0, 53n, 'RNDN').value, 2.0, 53n, 'RNDN');
 *     // → {value: 3.0 at prec 53, ternary: 0}
 *   mpfr_add_d(posZero(53n), -0, 53n, 'RNDD');
 *     // → {value: negZero(53n), ternary: 0}  — signed zero from -0
 */
export function mpfr_add_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Validate prec and rnd at the public boundary first.
  validateArgs(prec, rnd);

  // Step 1: convert the double `c` to an MPFR value at prec=53 bits.
  // This is always exact for all finite doubles (53 bits = full binary64
  // mantissa width). For NaN / ±Inf, mpfr_set_d returns the corresponding
  // MPFR special. The C source confirms exactness with MPFR_ASSERTD(inexact
  // == 0) after the mpfr_set_d call. We discard the ternary from set_d.
  //
  // Ref: mpfr/src/add_d.c L43-L44 — MPFR_TMP_INIT1 + mpfr_set_d call.
  const dMpfr = mpfr_set_d(c, IEEE_DBL_MANT_DIG, rnd).value;

  // Step 2: delegate to mpfr_add(b, dMpfr, prec, rnd). The ternary and
  // rounded value come entirely from mpfr_add; no further rounding.
  //
  // Ref: mpfr/src/add_d.c L46 — inexact = mpfr_add(a, b, d, rnd_mode).
  // Note: the C reference also calls mpfr_check_range, but for default
  // emin/emax (no range restriction active) mpfr_check_range is a no-op.
  return mpfr_add(b, dMpfr, prec, rnd);
}
