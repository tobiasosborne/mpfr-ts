/**
 * reference_ports/correct/mpfr_set_prec_raw.ts -- mutation-prove reference.
 *
 * Pure-TS lift of MPFR's internal mpfr_set_prec_raw: reset x's precision
 * field to `prec` WITHOUT reallocating and WITHOUT preserving the value.
 *
 * Algorithm (mpfr/src/set_prc_raw.c L25-L30), the entire C body:
 *
 *   void mpfr_set_prec_raw (mpfr_ptr x, mpfr_prec_t p) {
 *     MPFR_ASSERTN (MPFR_PREC_COND (p));                              // L27
 *     MPFR_ASSERTN (p <= (mpfr_prec_t) MPFR_GET_ALLOC_SIZE(x) * GMP_NUMB_BITS); // L28
 *     MPFR_PREC(x) = p;                                              // L29
 *   }
 *
 * The C function touches NO limb data: after the call the numeric value
 * of x is indeterminate (the old mantissa is reinterpreted under the new
 * precision). The ONLY well-defined post-condition is MPFR_PREC(x) == p.
 *
 * The immutable TS lift cannot mutate, and returning a full MPFR is
 * meaningless (the value is not preserved -- the re-interpreted mantissa
 * need not satisfy the MSB-normalisation invariant). So the port returns
 * the single well-defined observable: the resulting precision `prec`, as
 * a bigint. The input `x` is accepted to mirror the C contract (x is the
 * object whose prec is reset) but, exactly as in C, its value does not
 * affect the result.
 *
 * @divergence
 *   - Return: C is void + mutates x->_mpfr_prec; TS returns the new prec
 *     (bigint).
 *   - Alloc-fit precondition (L28): a C-level invariant about x's
 *     physical limb buffer. The immutable TS surface has no separate
 *     'allocation' field, so this is documented-but-not-enforced. The
 *     golden only emits p within the limb-rounded allocation bound.
 *   - Validity (L27): TS throws MPFRError('EPREC') on out-of-range prec
 *     (fail-fast, CLAUDE.md Rule 1) where C MPFR_ASSERTN aborts.
 *
 * Ref: mpfr/src/set_prc_raw.c L25-L30 -- the entire C reference.
 * Ref: src/core.ts -- PREC_MIN/PREC_MAX bounds; MPFR value type.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError, PREC_MAX, PREC_MIN, validate } from '../../../src/core.ts';

export function mpfr_set_prec_raw(x: MPFR, prec: bigint): bigint {
  // x must be a well-formed MPFR (the C side operates on a live mpfr_t);
  // validate it at the trust boundary even though its value is unused.
  validate(x);

  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  // MPFR_PREC_COND(p): 1 <= p <= MPFR_PREC_MAX. (L27)
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }

  // MPFR_PREC(x) = p; the resulting precision is exactly p. (L29)
  return prec;
}
