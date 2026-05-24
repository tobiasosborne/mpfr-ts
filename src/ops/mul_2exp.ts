/**
 * ops/mul_2exp.ts — pure-TS port of MPFR's `mpfr_mul_2exp`.
 *
 * Obsolete alias for `mpfr_mul_2ui`. The C body is a single-line delegate:
 *
 *   int mpfr_mul_2exp(mpfr_ptr y, mpfr_srcptr x, unsigned long int n, mpfr_rnd_t rnd_mode)
 *   {
 *     return mpfr_mul_2ui(y, x, n, rnd_mode);
 *   }
 *
 * Ref: mpfr/src/mul_2exp.c L28-L32 — the C reference body (single-line delegate).
 *
 * The TS port mirrors this faithfully: forward `(x, n, prec, rnd)` to
 * `mpfr_mul_2ui` unchanged. All invariants, validation, and rounding are
 * handled by `mpfr_mul_2ui`.
 *
 * C signature:
 *   int mpfr_mul_2exp(mpfr_ptr y, mpfr_srcptr x, unsigned long int n, mpfr_rnd_t rnd_mode)
 *
 * TS signature:
 *   mpfr_mul_2exp(x: MPFR, n: bigint, prec: bigint, rnd: RoundingMode): Result
 *
 * Refs:
 *   - mpfr/src/mul_2exp.c L28-L32 — the C reference (obsolete alias).
 *   - src/ops/mul_2ui.ts — the load-bearing delegate.
 *   - src/core.ts — locked schema (Result return shape).
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" — preserved
 *     through the delegate since mul_2ui handles it.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import { mpfr_mul_2ui } from "./mul_2ui.ts";

/**
 * Multiply `x` by `2^n` at precision `prec` rounded per `rnd`.
 *
 * This is an obsolete alias for `mpfr_mul_2ui`. The C implementation is
 * a single-line delegate; the TS port does the same.
 *
 * @mpfrName mpfr_mul_2exp
 *
 * @param x     the input MPFR value.
 * @param n     non-negative power of 2 (unsigned long in C; bigint >= 0n here).
 * @param prec  output precision in bits.
 * @param rnd   one of the five MPFR rounding modes.
 *
 * @returns `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on bad prec or negative `n`; `EROUND` on
 *                     unknown rounding mode. Delegated to `mpfr_mul_2ui`.
 */
export function mpfr_mul_2exp(
  x: MPFR,
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Ref: mpfr/src/mul_2exp.c L28-L32 — C body is `return mpfr_mul_2ui(y, x, n, rnd_mode);`
  return mpfr_mul_2ui(x, n, prec, rnd);
}
