/**
 * ops/sub_d.ts — pure-TS port of MPFR's `mpfr_sub_d`.
 *
 * Compute `b - c` where `b` is an {@link MPFR} value and `c` is a
 * JavaScript `number` (IEEE 754 binary64), rounded to `prec` bits per
 * `rnd`, returning the canonical `{value, ternary}` from src/core.ts.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sub_d(mpfr_ptr a, mpfr_srcptr b, double c, mpfr_rnd_t rnd_mode);
 *
 *   Ref: mpfr/src/sub_d.c L25–L50.
 *
 * Algorithm
 * ---------
 *
 * The C reference is a trivial two-step wrapper:
 *
 *   1. Stack-allocate an mpfr_t `d` at `IEEE_DBL_MANT_DIG` (53) bits.
 *   2. Call `mpfr_set_d(d, c, rnd_mode)` — always exact for finite doubles
 *      (53-bit IEEE mantissa fits in 53-bit MPFR), and exact for specials
 *      (NaN, ±Inf) — the ASSERTD checks `inexact == 0`.
 *   3. Call `mpfr_sub(a, b, d, rnd_mode)` and return its ternary.
 *
 * The TS port mirrors:
 *
 *   1. Convert `c` to MPFR at `prec = 53n` via `mpfr_set_d`. This is
 *      always exact (ternary 0 is the only possible result at 53 bits for
 *      any finite double, per the same ASSERTD).
 *   2. Delegate to `mpfr_sub(b, cMpfr, prec, rnd)`.
 *
 * The output precision of the final result is the caller-supplied `prec`,
 * NOT 53 — the intermediate `cMpfr` is merely at 53-bit precision before
 * it feeds into `mpfr_sub` which rounds the result to the target `prec`.
 *
 * Why converting `c` at 53 bits is always exact
 * ----------------------------------------------
 *
 * A finite IEEE 754 binary64 has at most 53 significant bits in its
 * mantissa (52 explicit + 1 implicit). `mpfr_set_d` at prec=53 is
 * therefore a lossless conversion — the intermediate MPFR captures the
 * double exactly. The sole rounding happens in the subsequent `mpfr_sub`,
 * from the exact result to the `prec`-bit output.
 *
 * This matches the C side's `MPFR_ASSERTD (inexact == 0)` assertion at
 * mpfr/src/sub_d.c L36.
 *
 * NaN / ±Inf / ±0 handling
 * ------------------------
 *
 * All specials are handled by the delegates:
 *   - `mpfr_set_d(NaN, 53n, ...)` → NAN_VALUE (ternary 0)
 *   - `mpfr_set_d(Inf, 53n, ...)` → posInf/negInf at 53n (ternary 0)
 *   - `mpfr_set_d(±0, 53n, ...)` → posZero/negZero at 53n (ternary 0)
 *
 * Then `mpfr_sub(b, cMpfr, prec, rnd)` routes through its own
 * special-value dispatch and handles Inf±Inf → NaN, NaN propagation,
 * zero-cancellation sign rules per rounding mode, etc.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub_d.c L25–L50 — C reference body.
 *   - src/ops/set_d.ts — `mpfr_set_d`: double → MPFR at 53n (exact).
 *   - src/ops/sub.ts  — `mpfr_sub`: MPFR subtraction with correct rounding.
 *   - src/core.ts     — locked schema (MPFR, RoundingMode, Result, Ternary).
 *   - CLAUDE.md "Hallucination-risk callouts: NaN != NaN" — NaN handled
 *     by delegates, not by this port directly.
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *     RNDD of `x - x` → `-0`; signed zero preservation flows through
 *     `mpfr_set_d` (for c=±0) and through `mpfr_sub` (for the result sign).
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import { MPFRError, PREC_MAX, PREC_MIN } from "../core.ts";
import { mpfr_set_d } from "./set_d.ts";
import { mpfr_sub } from "./sub.ts";

/**
 * Validate the public-boundary scalar arguments. Throws `MPFRError` on
 * bad `prec` or `rnd`. Note: `c` is a raw `number` and every double is a
 * valid input (NaN, Inf, signed zero are all handled), so we never throw
 * on `c`.
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
 * Compute `b - c` where `b` is an MPFR value and `c` is an IEEE 754
 * binary64, rounding the result to `prec` bits per `rnd`.
 *
 * @mpfrName mpfr_sub_d
 *
 * @param b    Minuend — an MPFR value of any kind.
 * @param c    Subtrahend — a JavaScript `number`. NaN, ±Infinity, and ±0
 *             are all valid (they convert to the corresponding MPFR
 *             singular values at precision 53 before the subtract).
 * @param prec Output precision in **bits** (not decimal digits), in
 *             `[PREC_MIN, PREC_MAX]`. The intermediate MPFR for `c` is
 *             always at 53 bits (exact for any double); the output is
 *             rounded to this `prec`.
 * @param rnd  One of the five {@link RoundingMode} values.
 *
 * @returns    `{value, ternary}` where:
 *             - `value` is the MPFR result at `prec` bits, passing
 *               `validate()` without post-processing;
 *             - `ternary` is the sign of `(rounded - exact)`, `0` for
 *               exact results (e.g. specials producing Inf or NaN).
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. Bad `c` (NaN, Inf, ±0) is NOT an error.
 *
 * @example
 *   // 3.14 - 1.0 = 2.14 (approximate — likely inexact at prec 53)
 *   mpfr_sub_d(setD(3.14, 53n, 'RNDN').value, 1.0, 53n, 'RNDN');
 *
 *   // NaN propagation: b = NaN → result NaN
 *   mpfr_sub_d(NAN_VALUE, 1.0, 53n, 'RNDN');  // {value: NAN_VALUE, ternary: 0}
 *
 *   // Double NaN subtrahend: c = NaN → cMpfr = NAN_VALUE → NaN result
 *   mpfr_sub_d(setD(1.0, 53n, 'RNDN').value, NaN, 53n, 'RNDN');
 */
export function mpfr_sub_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Validate the precision and rounding mode at the public boundary.
  // `c` is always a valid input — the delegate `mpfr_set_d` handles
  // all special double values (NaN, ±Inf, ±0) without error.
  validateArgs(prec, rnd);

  // Step 1: convert the double `c` to MPFR at exactly 53 bits.
  //
  // Ref: mpfr/src/sub_d.c L34–L36 — MPFR_TMP_INIT1 + mpfr_set_d at
  //   IEEE_DBL_MANT_DIG (53) bits; MPFR_ASSERTD(inexact == 0) asserts
  //   the set is always exact at this precision.
  //
  // The rounding mode passed here doesn't matter in practice (the set
  // is exact at 53 bits for any finite double, and specials are exact
  // by definition), but we mirror the C call signature which passes
  // `rnd_mode` to `mpfr_set_d`. This is consistent and safe.
  const { value: cMpfr } = mpfr_set_d(c, 53n, rnd);

  // Step 2: delegate to mpfr_sub.
  //
  // Ref: mpfr/src/sub_d.c L39 — `inexact = mpfr_sub(a, b, d, rnd_mode)`.
  //
  // The output precision is the caller-supplied `prec`, not 53. The
  // intermediate `cMpfr` is merely the exact MPFR representation of the
  // double; the rounding to `prec` bits happens inside `mpfr_sub`.
  return mpfr_sub(b, cMpfr, prec, rnd);
}
