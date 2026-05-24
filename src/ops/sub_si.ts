/**
 * ops/sub_si.ts — pure-TS port of MPFR's `mpfr_sub_si`.
 *
 * Trivial dispatcher: routes to {@link mpfr_sub_ui} for `c >= 0` and
 * {@link mpfr_add_ui} for `c < 0`. The exact mirror of `mpfr_add_si`
 * with the subtraction/addition branches swapped.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sub_si(mpfr_ptr y, mpfr_srcptr x, long u, mpfr_rnd_t rnd);
 *
 *   Body (mpfr/src/si_op.c L48-L65):
 *
 *     if (u >= 0)
 *         res = mpfr_sub_ui(y, x, u, rnd_mode);
 *     else
 *         res = mpfr_add_ui(y, x, -(unsigned long) u, rnd_mode);
 *     return res;
 *
 *   The C cast `-(unsigned long) u` is needed when `u == LONG_MIN` to
 *   avoid signed overflow; two's-complement unsigned negation gives
 *   `2^63`. The TS port avoids the issue entirely: bigint negation is
 *   overflow-free, and `-c` for `c == LONG_MIN_VAL` produces `2^63`,
 *   which fits in add_ui's `[0, ULONG_MAX]` range.
 *
 *   Ref: mpfr/src/si_op.c L48-L65.
 *
 * TS signature
 * ------------
 *
 *   mpfr_sub_si(b: MPFR, c: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - `c` is a `bigint` in `[LONG_MIN, LONG_MAX]` (`[-2^63, 2^63 - 1]`).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/si_op.c L48-L65 — C reference.
 *   - src/ops/sub_ui.ts — c >= 0 branch.
 *   - src/ops/add_ui.ts — c < 0 branch.
 *   - src/ops/add_si.ts — structural mirror (add instead of sub).
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — bigint negation is
 *     overflow-free; no need for the C-style unsigned-cast workaround.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import { MPFRError } from "../core.ts";
import { mpfr_add_ui } from "./add_ui.ts";
import { mpfr_sub_ui } from "./sub_ui.ts";

/** Smallest signed 64-bit integer: -(2^63). */
const LONG_MIN_VAL: bigint = -(1n << 63n);
/** Largest signed 64-bit integer: 2^63 - 1. */
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

function validateC(c: bigint): void {
  if (typeof c !== 'bigint') {
    throw new MPFRError('EPREC', `c must be bigint, got ${typeof c}`);
  }
  if (c < LONG_MIN_VAL || c > LONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `c out of int64 range [${LONG_MIN_VAL}, ${LONG_MAX_VAL}], got ${c}`,
    );
  }
}

/**
 * Compute `b - c` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_sub_si
 *
 * @param b     the MPFR minuend.
 * @param c     the signed integer subtrahend, in `[LONG_MIN, LONG_MAX]`.
 * @param prec  output precision in bits.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}`.
 *
 * @throws {MPFRError} `EPREC` on bad `c`; downstream `EPREC` / `EROUND`
 *                    on bad prec / rnd.
 */
export function mpfr_sub_si(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateC(c);
  // Ref: mpfr/src/si_op.c L57-L62 — dispatch on sign of u.
  if (c >= 0n) {
    // c >= 0: b - c = sub_ui(b, c)
    return mpfr_sub_ui(b, c, prec, rnd);
  }
  // c < 0: b - c = b + (-c) = add_ui(b, -c).
  // -c is positive; -LONG_MIN_VAL = 2^63 fits in add_ui's [0, 2^64 - 1] range.
  return mpfr_add_ui(b, -c, prec, rnd);
}
