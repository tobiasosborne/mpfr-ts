/**
 * ops/add_si.ts — pure-TS port of MPFR's `mpfr_add_si`.
 *
 * Add an {@link MPFR} value `b` and a signed integer `c` (representing a
 * C `long`), returning the rounded sum at the target precision.
 *
 * C signature
 * -----------
 *
 *   int mpfr_add_si(mpfr_ptr y, mpfr_srcptr x, long int u, mpfr_rnd_t rnd_mode);
 *
 *   Body (mpfr/src/si_op.c L29-L46):
 *
 *     if (u >= 0)
 *       res = mpfr_add_ui(y, x, u, rnd_mode);
 *     else
 *       res = mpfr_sub_ui(y, x, -(unsigned long) u, rnd_mode);
 *
 *   Ref: mpfr/src/si_op.c L29-L46 — the C reference.
 *
 * TS signature
 * ------------
 *
 *   mpfr_add_si(b: MPFR, c: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - `c` is a signed `bigint` in `[LONG_MIN, LONG_MAX]` = `[-(2^63), 2^63 - 1]`.
 *   - Returns `Result` per src/core.ts.
 *
 * TS divergences
 * --------------
 *
 *   1. TS bigint negation is overflow-free: `-c` for c == LONG_MIN = -2^63 gives
 *      exactly 2^63, which is within uint64 range. C must cast through
 *      `(unsigned long)` to handle this; TS needs no special case.
 *   2. The C `mpfr_set_prec` / allocation pattern is replaced by the idiomatic
 *      TS `Result` return shape.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/si_op.c L29-L46 — C reference (the add_si function).
 *   - src/ops/add_ui.ts — c >= 0 branch delegate.
 *   - src/ops/sub_ui.ts — c < 0 branch delegate.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts: TS bigint negation is safe (no
 *     overflow); C must cast through unsigned to handle LONG_MIN."
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from "../core.ts";
import { mpfr_add_ui } from "./add_ui.ts";
import { mpfr_sub_ui } from "./sub_ui.ts";

/**
 * C `long` range: [-2^63, 2^63 - 1] (64-bit LP64 platforms, which is what
 * MPFR's goldens are generated on).
 * Ref: mpfr/src/si_op.c L30 — parameter type `long int u`.
 */
const LONG_MIN_VAL: bigint = -(1n << 63n);
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

function validateArgs(
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof c !== 'bigint') {
    throw new MPFRError('EPREC', `c must be bigint, got ${typeof c}`);
  }
  if (c < LONG_MIN_VAL || c > LONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `c out of signed long range [${LONG_MIN_VAL}, ${LONG_MAX_VAL}], got ${c}`,
    );
  }
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
 * Compute `b + c` at precision `prec` per `rnd`, where `c` is a signed
 * integer in the range of a C `long`.
 *
 * @mpfrName mpfr_add_si
 *
 * @param b     the MPFR operand.
 * @param c     the signed integer operand, in `[-(2^63), 2^63 - 1]`.
 * @param prec  output precision in bits.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}`.
 *
 * @throws {MPFRError} `EPREC` on bad `c` / `prec`; `EROUND` on bad rnd.
 */
export function mpfr_add_si(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(c, prec, rnd);

  // Ref: mpfr/src/si_op.c L40-L43 — dispatch on sign of u.
  if (c >= 0n) {
    // Non-negative: delegate to mpfr_add_ui directly.
    // c is in [0, LONG_MAX] = [0, 2^63 - 1], within uint64 range.
    return mpfr_add_ui(b, c, prec, rnd);
  } else {
    // Negative: delegate to mpfr_sub_ui with magnitude.
    // TS bigint negation: -c for c == LONG_MIN = -2^63 gives 2^63,
    // which is within uint64 range. No overflow, unlike C.
    // Ref: mpfr/src/si_op.c L43 — `mpfr_sub_ui(y, x, -(unsigned long) u, rnd)`.
    return mpfr_sub_ui(b, -c, prec, rnd);
  }
}
