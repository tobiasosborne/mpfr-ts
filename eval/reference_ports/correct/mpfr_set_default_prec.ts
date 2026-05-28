/**
 * reference_ports/correct/mpfr_set_default_prec.ts -- mutation-prove
 * reference for mpfr_set_default_prec.
 *
 * Per CLAUDE.md PIL.3 the calibration baseline. The production
 * src/ops/set_default_prec.ts does not yet exist.
 *
 * Algorithm (mpfr/src/set_dfl_prec.c L28-L33):
 *   MPFR_ASSERTN(MPFR_PREC_COND(prec));   // prec in [PREC_MIN, PREC_MAX]
 *   __gmpfr_default_fp_bit_precision = prec;
 *
 * Immutable-API lift: return the resulting default precision (== prec for
 * every valid call). The out-of-range C abort() is lifted to a thrown
 * MPFRError('EPREC', ...) -- consistent with src/core.ts assertPrec.
 * The golden only feeds valid precs in [1, PREC_MAX] (PREC_MAX here is
 * the core.ts ceiling 2^31 - 257), so the throw path is exercised by the
 * runner's n_throw bookkeeping, not by these value goldens.
 *
 * prec is in BITS.
 *
 * Ref: mpfr/src/set_dfl_prec.c L28-L33 -- C reference.
 * Ref: src/core.ts L216-L236 -- PREC_MIN / PREC_MAX bounds.
 * Ref: eval/functions/mpfr_set_default_prec/spec.json -- contract.
 */

import { MPFRError, PREC_MIN, PREC_MAX } from '../../../src/core.ts';

export function mpfr_set_default_prec(prec: bigint): bigint {
  if (typeof prec !== 'bigint') {
    throw new MPFRError(
      'EPREC',
      `mpfr_set_default_prec: prec must be bigint, got ${typeof prec}`,
    );
  }
  // C MPFR_ASSERTN(MPFR_PREC_COND(prec)) -> lift to a recoverable throw.
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  // Store verbatim; the resulting default precision equals prec.
  return prec;
}
