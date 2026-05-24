/**
 * ops/set_flt.ts — pure-TS port of MPFR's `mpfr_set_flt`.
 *
 * Convert a machine single-precision (binary32) float to an {@link MPFR}
 * value at the caller-supplied precision, with correct rounding per the
 * rounding mode, returning the canonical `{value, ternary}` shape.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_flt(mpfr_ptr r, float f, mpfr_rnd_t rnd_mode);
 *
 *   Ref: mpfr/src/set_flt.c L25–L32 — the entire body is:
 *
 *     return mpfr_set_d(r, (double) f, rnd_mode);
 *
 *   Rationale from the C comment: "we convert f to double precision and
 *   use mpfr_set_d; NaN and infinities should be preserved (except the
 *   sign bit for NaN), and all single precision numbers are exactly
 *   representable in the double format, thus the conversion is always
 *   exact."
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_flt(f: number, prec: bigint, rnd: RoundingMode): Result;
 *
 * Design rationale
 * ----------------
 *
 * TypeScript (and JavaScript) have no distinct binary32 type. Every JS
 * `number` is already an IEEE 754 binary64. A C caller passes a `float`
 * which the compiler promotes to `double` when calling `mpfr_set_d`; on
 * the TS side the caller already holds a binary64 (the binary32→binary64
 * promotion has already occurred implicitly before the value reaches this
 * function).
 *
 * Therefore the TS port of `mpfr_set_flt` is identical to `mpfr_set_d`
 * with the same argument: delegate unconditionally. There is no TS-level
 * distinction to implement.
 *
 * The golden_driver.c receives a C `float`, promotes it to `double`, and
 * emits it on the wire as a double. This port receives that same double
 * and routes it through `mpfr_set_d`, which is exactly what the C
 * reference does.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_flt.c L25–L32 — C reference (one-line delegate).
 *   - src/ops/set_d.ts — the load-bearing delegate.
 *   - src/core.ts — value model (MPFR, Result, RoundingMode).
 *   - CLAUDE.md "Hallucination-risk callouts: NaN != NaN" — NaN
 *     propagates silently; we don't special-case it here (set_d handles it).
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *     set_d preserves ±0; we inherit that behaviour by delegation.
 */

import type { Result, RoundingMode } from "../core.ts";
import { mpfr_set_d } from "./set_d.ts";

/**
 * Convert a JavaScript `number` (representing an IEEE 754 binary32 value
 * on the wire, already promoted to binary64) to an {@link MPFR} value at
 * `prec` bits, rounded per `rnd`.
 *
 * @mpfrName mpfr_set_flt
 *
 * @param f     the float value (as a JS number / IEEE 754 binary64). NaN,
 *              ±Infinity, and ±0 are all handled; signed zero is preserved.
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five rounding modes in {@link RoundingMode}.
 *
 * @returns     a {@link Result} pair `{value, ternary}`.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. These are delegated to `mpfr_set_d`.
 */
export function mpfr_set_flt(f: number, prec: bigint, rnd: RoundingMode): Result {
  // Ref: mpfr/src/set_flt.c L25–L32 — delegate to mpfr_set_d after
  // implicitly promoting the float to double. On the TS side there is no
  // distinct binary32 type; f is already a binary64 holding the exact
  // value of whatever binary32 the C driver passed on the wire.
  return mpfr_set_d(f, prec, rnd);
}
