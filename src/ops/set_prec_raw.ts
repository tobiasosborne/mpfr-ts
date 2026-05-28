/**
 * mpfr_set_prec_raw.ts -- pure-TS port of MPFR's `mpfr_set_prec_raw`.
 *
 * Resets the precision field of x to `prec` WITHOUT reallocating the
 * mantissa limb buffer and WITHOUT preserving the numeric value.
 * The C body (mpfr/src/set_prc_raw.c L25-L30) is three lines: two
 * MPFR_ASSN preconditions followed by MPFR_PREC(x) = p.
 *
 * Because the TS surface has no separate allocation concept from prec,
 * the alloc-fit precondition (p <= alloc_size * GMP_NUMB_BITS) is not
 * surfaceable here; the golden driver respects it. The only graded
 * post-condition is that the returned precision equals p.
 *
 * Ref: mpfr/src/set_prc_raw.c L25-L30 -- entire C body.
 * Ref: mpfr/src/mpfr-impl.h L975 -- MPFR_PREC(x) macro.
 * Ref: src/core.ts PREC_MIN, PREC_MAX, MPFRError -- precision bounds.
 */

import type { MPFR } from "../core.ts";
import { MPFRError, PREC_MIN, PREC_MAX } from "../core.ts";

/**
 * Sets the precision of `x` to `prec` without reallocating or preserving
 * the value. Returns the new precision.
 *
 * @param x    An MPFR value (value is irrelevant; only the precision field
 *             is conceptually being reset, but in the immutable TS lift we
 *             return the new precision as a scalar).
 * @param prec New precision in bits. Must satisfy PREC_MIN <= prec <= PREC_MAX.
 * @returns    The new precision `prec` as a bigint.
 * @throws     {MPFRError} `EPREC` if `prec` is out of range.
 */
export function mpfr_set_prec_raw(x: MPFR, prec: bigint): bigint {
  // C: MPFR_ASSERTN(MPFR_PREC_COND(p)) -- p is a valid precision.
  if (typeof prec !== "bigint" || prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError(
      "EPREC",
      `mpfr_set_prec_raw: invalid precision ${prec}; must be in [${PREC_MIN}, ${PREC_MAX}]`,
    );
  }

  // C: MPFR_PREC(x) = p -- the single store.
  // In the TS lift there is no mutable rop; we return the new precision
  // as the observable post-condition (the golden asserts only this).
  return prec;
}
