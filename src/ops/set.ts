/**
 * ops/set.ts — pure-TS port of MPFR's `mpfr_set`.
 *
 * The canonical "copy b into a, rounding to a's precision" entry point.
 * Public API; the most frequent way users move values between precisions
 * in MPFR-using code. In the C reference, mpfr_set is a one-line delegate
 * to mpfr_set4 with the source's own sign:
 *
 *     int mpfr_set (mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode) {
 *         return mpfr_set4 (a, b, rnd_mode, MPFR_SIGN (b));
 *     }
 *
 * Ref: mpfr/src/set.c L67-L79.
 *
 * The TS port preserves the same shape: it forwards (b, prec, rnd, b.sign)
 * to mpfr_set4. Idiomatic TS surface imports the locked `Result` shape
 * from `src/core.ts`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set(mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode);
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_set(b, prec, rnd) -> Result
 *
 *   - `prec` is the target output precision in bits (the C side reads
 *     this from `a`); explicit here per the immutable-API convention.
 *   - Returns `{value, ternary}` per the locked schema.
 *
 * Semantics by kind of `b`
 * ------------------------
 *
 *   - NaN: schema canonicalises to `NAN_VALUE` (sign=1, prec=0n).
 *     Ternary 0. The C side returns ternary 0 too, sets ERANGE; we don't
 *     have a flag side-channel and drop the ERANGE marker.
 *   - Inf: preserve sign, change prec. Ternary 0.
 *   - Zero: preserve sign (signed zero is observable). Ternary 0.
 *   - Normal: delegate to mpfr_set4 with `signb = b.sign`. Same-prec
 *     copy is exact (ternary 0); larger prec is exact (lossless pad);
 *     smaller prec rounds per `rnd` (ternary -1/0/+1).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set.c L67-L79 — canonical C body.
 *   - src/ops/set4.ts — load-bearing delegate.
 *   - src/core.ts — locked schema (MPFR, RoundingMode, Result, Sign).
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real".
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is sign of
 *     (rounded - exact)".
 */

import type { MPFR, Result, RoundingMode } from "/home/tobias/Projects/mpfr-ts/src/core.ts";
import { mpfr_set4 } from "/home/tobias/Projects/mpfr-ts/src/ops/set4.ts";

/**
 * Copy `b` into a fresh MPFR value at the target `prec`, rounding per
 * `rnd`. The output sign matches `b.sign` (this is the property that
 * distinguishes `mpfr_set` from its siblings `mpfr_abs`, `mpfr_neg`,
 * `mpfr_setsign`, `mpfr_copysign`, all of which delegate to
 * `mpfr_set4` with a different `signb`).
 *
 * @mpfrName mpfr_set
 *
 * @param b    Source value (any kind).
 * @param prec Output precision in **bits**, in [PREC_MIN, PREC_MAX].
 * @param rnd  One of the five RoundingMode values.
 *
 * @returns `{ value, ternary }` — the copied/rounded value and the
 *          ternary flag (sign of rounded - exact).
 *
 * @throws {MPFRError} `EPREC` on out-of-range precision; `EROUND` on
 *                     unknown rounding mode.
 *
 * Ref: mpfr/src/set.c L67-L79 — C reference (delegates to mpfr_set4).
 */
export function mpfr_set(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Delegate to mpfr_set4 with signb = MPFR_SIGN(b).
  // Ref: mpfr/src/set.c L78 — `return mpfr_set4(a, b, rnd_mode, MPFR_SIGN(b));`.
  //
  // For NaN, b.sign is fixed to 1 by the TS schema (src/core.ts).
  // mpfr_set4 discards signb on NaN and returns NAN_VALUE; the schema's
  // sign=1 is the convention. The same call shape works uniformly.
  return mpfr_set4(b, prec, rnd, b.sign);
}
