/**
 * ops/div_2exp.ts — pure-TS port of MPFR's `mpfr_div_2exp`.
 *
 * This is an OBSOLETE alias for `mpfr_div_2ui`. The entire C body is a
 * single-line delegate:
 *
 *   int mpfr_div_2exp(mpfr_ptr y, mpfr_srcptr x, unsigned long int n,
 *                     mpfr_rnd_t rnd_mode)
 *   {
 *     return mpfr_div_2ui(y, x, n, rnd_mode);
 *   }
 *
 * Ref: mpfr/src/div_2exp.c L28–L32 — the C reference body (single-line
 *   delegate to mpfr_div_2ui).
 *
 * The TS port mirrors identically: forward all arguments to
 * `mpfr_div_2ui` unchanged and return its `Result` directly.
 * No additional logic, no additional validation — div_2ui already
 * handles all precondition checks, singular propagation, and rounding.
 *
 * Ref: src/ops/div_2ui.ts — the load-bearing delegate (signed/unsigned
 *   argument is structurally identical; n is a non-negative bigint).
 * Ref: src/core.ts — locked schema (Result, MPFR, RoundingMode).
 * Ref: CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *   sign is preserved through the 2^-n division via div_2ui.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import { mpfr_div_2ui } from "./div_2ui.ts";

/**
 * Divide `x` by `2^n`, rounding to `prec` bits under `rnd`.
 *
 * Obsolete alias for {@link mpfr_div_2ui}. Delegates unconditionally.
 *
 * @mpfrName mpfr_div_2exp
 *
 * @param x     the input MPFR value.
 * @param n     non-negative exponent as a bigint in `[0, 2^64 - 1]`.
 * @param prec  output precision in bits.
 * @param rnd   one of the five MPFR rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on bad `n` or `prec`; `EROUND` on bad
 *                    rounding mode — propagated from mpfr_div_2ui.
 */
export function mpfr_div_2exp(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Ref: mpfr/src/div_2exp.c L31 — `return mpfr_div_2ui(y, x, n, rnd_mode);`
  return mpfr_div_2ui(x, n, prec, rnd);
}
