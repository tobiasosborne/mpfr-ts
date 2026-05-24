/**
 * ops/mul_si.ts — pure-TS port of MPFR's `mpfr_mul_si`.
 *
 * Multiply an {@link MPFR} value `b` by a signed long integer `c`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul_si(mpfr_ptr y, mpfr_srcptr x, long int u,
 *                   mpfr_rnd_t rnd_mode);
 *
 *   Ref: mpfr/src/si_op.c L90-L112.
 *
 * Algorithm
 * ---------
 *
 * The C reference dispatches on the sign of `u` (the integer argument):
 *
 *   if (u >= 0)
 *     res = mpfr_mul_ui(y, x, u, rnd_mode);
 *   else {
 *     res = -mpfr_mul_ui(y, x, -(unsigned long)u, MPFR_INVERT_RND(rnd_mode));
 *     MPFR_CHANGE_SIGN(y);
 *   }
 *
 *   Ref: mpfr/src/si_op.c L102-L111.
 *
 * For c < 0 the rounding mode must be inverted before calling mul_ui, and
 * then both the sign of the result AND the ternary are negated. This is
 * because rounding x * |c| under RNDU, then negating, is semantically
 * equivalent to rounding -(x * |c|) under RNDD — and the ternary sign of
 * (rounded - exact) inverts with the value sign.
 *
 * MPFR_INVERT_RND: RNDU <-> RNDD, RNDN/RNDZ/RNDA are unchanged.
 *   Ref: mpfr/src/mpfr-impl.h L1249-L1250.
 *
 * The sign flip via MPFR_CHANGE_SIGN applies to all non-NaN kinds (normal,
 * zero, inf). For NaN, the schema fixes sign=1 — no-op required.
 *
 * TS signature
 * ------------
 *
 *   mpfr_mul_si(b: MPFR, c: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 * where `c` is in `[LONG_MIN, LONG_MAX]` (i.e. `[-2^63, 2^63 - 1]`).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/si_op.c L90-L112 — the C reference.
 *   - mpfr/src/mpfr-impl.h L1249-L1250 — MPFR_INVERT_RND macro.
 *   - src/ops/mul_ui.ts — load-bearing delegate.
 *   - src/ops/neg.ts — the rounding-vs-sign reasoning is parallel.
 *   - src/core.ts — locked schema; Sign / Ternary.
 *   - CLAUDE.md "Hallucination-risk callouts: Ternary flag is the sign
 *     of (rounded - exact)" — sign flip inverts ternary direction.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from "../core.ts";
import { MPFRError, PREC_MAX, PREC_MIN } from "../core.ts";
import { mpfr_mul_ui } from "./mul_ui.ts";

/** Signed long range: [-2^63, 2^63 - 1]. */
const LONG_MIN: bigint = -(1n << 63n);
const LONG_MAX: bigint = (1n << 63n) - 1n;

/**
 * Invert the rounding mode for the c < 0 negation trick.
 *
 * RNDU <-> RNDD; all others (RNDN, RNDZ, RNDA) are self-inverse.
 *
 * Ref: mpfr/src/mpfr-impl.h L1249-L1250 — MPFR_INVERT_RND macro.
 */
function invertRnd(rnd: RoundingMode): RoundingMode {
  if (rnd === 'RNDU') return 'RNDD';
  if (rnd === 'RNDD') return 'RNDU';
  return rnd;
}

/**
 * Negate the sign field of an MPFR value (MPFR_CHANGE_SIGN equivalent).
 *
 * For NaN the schema forces sign=1, so we leave it unchanged.
 * For normal/zero/inf we flip the sign.
 *
 * Ref: mpfr/src/si_op.c L108 — `MPFR_CHANGE_SIGN(y)`.
 */
function changeSign(v: MPFR): MPFR {
  if (v.kind === 'nan') {
    // NaN sign is fixed at 1 by schema convention; no change.
    return v;
  }
  const newSign: Sign = (v.sign === 1 ? -1 : 1) as Sign;
  return {
    kind: v.kind,
    sign: newSign,
    prec: v.prec,
    exp: v.exp,
    mant: v.mant,
  };
}

function validateArgs(c: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof c !== 'bigint') {
    throw new MPFRError('EPREC', `c must be bigint, got ${typeof c}`);
  }
  if (c < LONG_MIN || c > LONG_MAX) {
    throw new MPFRError(
      'EPREC',
      `c out of signed long range [${LONG_MIN}, ${LONG_MAX}], got ${c}`,
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
 * Multiply `b` by the signed integer `c` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_mul_si
 *
 * @param b     the MPFR multiplicand.
 * @param c     signed integer in `[-(2^63), 2^63 - 1]` as `bigint`.
 * @param prec  output precision in bits.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}`.
 *
 * @throws {MPFRError} `EPREC` on bad `c` / `prec`; `EROUND` on bad rnd.
 */
export function mpfr_mul_si(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(c, prec, rnd);

  // --- Fast path: c >= 0 --------------------------------------------------
  // Ref: mpfr/src/si_op.c L102-L103.
  if (c >= 0n) {
    return mpfr_mul_ui(b, c, prec, rnd);
  }

  // --- Negative c path ----------------------------------------------------
  // Ref: mpfr/src/si_op.c L104-L111.
  //
  //   res = -mpfr_mul_ui(y, x, -(unsigned long)u, MPFR_INVERT_RND(rnd));
  //   MPFR_CHANGE_SIGN(y);
  //
  // We compute b * (-c) with the inverted rounding mode, then:
  //   1. Flip the result value's sign (MPFR_CHANGE_SIGN).
  //   2. Negate the ternary (the `res = -...` in C).
  //
  // The inversion of the rounding mode is necessary because:
  //   round(b * (-c), RNDD) = -round(b * |c|, RNDU)
  // and the ternary obeys the same inversion:
  //   ternary(b * (-c), RNDD) = -ternary(b * |c|, RNDU)

  const absC = -c;  // c < 0, so absC > 0 — valid for mul_ui
  const invertedRnd = invertRnd(rnd);

  // mpfr_mul_ui requires c in [0, 2^64 - 1]; absC = -c, and since
  // c >= LONG_MIN = -2^63, absC <= 2^63, which is within uint64 range.
  const { value: rawValue, ternary: rawTernary } = mpfr_mul_ui(b, absC, prec, invertedRnd);

  // Flip sign (MPFR_CHANGE_SIGN(y)).
  const value = changeSign(rawValue);

  // Negate the ternary (`res = -res` in C).
  // Ref: mpfr/src/si_op.c L106-L107.
  const ternary: Ternary = (-rawTernary) as Ternary;

  return { value, ternary };
}
