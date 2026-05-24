/**
 * ops/dim.ts — pure-TS port of MPFR's `mpfr_dim`.
 *
 * Positive difference function: `dim(b, c) = max(0, b - c)`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_dim(mpfr_ptr z, mpfr_srcptr x, mpfr_srcptr y, mpfr_rnd_t rnd_mode)
 *
 * TS signature
 * ------------
 *
 *   mpfr_dim(b: MPFR, c: MPFR, prec: bigint, rnd: RoundingMode): Result
 *
 * Algorithm
 * ---------
 *
 * Direct translation of mpfr/src/dim.c L24–L47:
 *
 *   1. If b or c is NaN → return NaN (ternary 0).
 *      NaN check MUST come before the comparison (compareMPFR returns null
 *      for NaN, but we check explicitly here to mirror the C order and
 *      avoid any confusion). Ref: CLAUDE.md "NaN ≠ NaN".
 *
 *   2. If b > c → return mpfr_sub(b, c, prec, rnd).
 *      Ref: mpfr/src/dim.c L607 — `return mpfr_sub(z, x, y, rnd_mode);`
 *
 *   3. Otherwise (b ≤ c, including b == c): return +0 at prec, ternary 0.
 *      The result is always +0 (never -0), regardless of rounding mode.
 *      Ref: mpfr/src/dim.c L609–L613 — `MPFR_SET_ZERO(z); MPFR_SET_POS(z);`
 *
 * Refs
 * ----
 *
 *   - mpfr/src/dim.c L24–L47 — the C reference body.
 *   - src/ops/sub.ts — load-bearing delegate for the b > c branch.
 *   - src/internal/mpfr/cmp_raw.ts — non-throwing comparison (returns null for NaN).
 *   - src/core.ts — locked schema; posZero for the zero branch; NAN_VALUE for NaN.
 *   - CLAUDE.md "Hallucination-risk callouts": NaN ≠ NaN — NaN dispatch before cmp.
 */

import type { MPFR, RoundingMode, Result } from "../core.ts";
import { MPFRError, NAN_VALUE, PREC_MIN, PREC_MAX, posZero } from "../core.ts";
import { mpfr_sub } from "./sub.ts";
import { compareMPFR } from "../internal/mpfr/cmp_raw.ts";

/**
 * Validate the scalar boundary arguments (prec, rnd).
 * Mirrors the pattern in src/ops/sub.ts and src/ops/add.ts.
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
 * Positive difference: `dim(b, c) = max(0, b - c)`.
 *
 * @mpfrName mpfr_dim
 *
 * @param b   first operand. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param c   second operand. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns `{value, ternary}`.
 *   - If b or c is NaN: `{value: NAN_VALUE, ternary: 0}`.
 *   - If b > c: result of `mpfr_sub(b, c, prec, rnd)`.
 *   - If b <= c: `{value: posZero(prec), ternary: 0}` (exact +0).
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *
 * @example
 *   dim(setD(3.0, 53n, 'RNDN').value, setD(1.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 2.0 at prec 53, ternary: 0}
 *   dim(setD(1.0, 53n, 'RNDN').value, setD(3.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: posZero(53n), ternary: 0}
 *   dim(NAN_VALUE, x, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}
 */
export function mpfr_dim(
  b: MPFR,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // Step 1: NaN dispatch FIRST — before any comparison.
  // Ref: mpfr/src/dim.c L601–L605 — `if (MPFR_IS_NAN(x) || MPFR_IS_NAN(y)) { MPFR_SET_NAN(z); MPFR_RET_NAN; }`
  // We check kind directly (not via compareMPFR) to mirror the C guard order.
  if (b.kind === 'nan' || c.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // Step 2: Compare b and c. Both are non-NaN at this point, so compareMPFR
  // will never return null. Ref: mpfr/src/dim.c L607 — `if (mpfr_cmp(x, y) > 0)`.
  const cmpResult = compareMPFR(b, c);
  // cmpResult is null only if either operand is NaN, which we've excluded above.
  // The null branch is structurally unreachable here but we guard defensively.
  if (cmpResult === null) {
    return { value: NAN_VALUE, ternary: 0 };
  }

  if (cmpResult > 0) {
    // b > c: return b - c, rounded per rnd.
    // Ref: mpfr/src/dim.c L607 — `return mpfr_sub(z, x, y, rnd_mode);`
    return mpfr_sub(b, c, prec, rnd);
  } else {
    // b <= c (includes b == c and b < c): return exact +0.
    // The result is ALWAYS +0 (positive zero), regardless of rounding mode.
    // Ref: mpfr/src/dim.c L609–L613 — `MPFR_SET_ZERO(z); MPFR_SET_POS(z); MPFR_RET(0);`
    return { value: posZero(prec), ternary: 0 };
  }
}
