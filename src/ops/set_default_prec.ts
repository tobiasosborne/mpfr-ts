/**
 * port.ts -- mpfr_set_default_prec
 *
 * Pure-TS port of MPFR's mpfr_set_default_prec (mpfr/src/set_dfl_prec.c
 * L28-L33). The C function asserts PREC_PREC_COND(prec) then stores prec
 * into the thread-global __gmpfr_default_fp_bit_precision.
 *
 * Immutable-API lift: return the resulting default precision (== prec for
 * every valid call). The out-of-range C abort() is lifted to a thrown
 * MPFRError('EPREC', ...) consistent with src/core.ts assertPrec.
 *
 * Ref: mpfr/src/set_dfl_prec.c L28-L33 -- C reference (assert + store).
 * Ref: src/core.ts L216-L236 -- PREC_MIN / PREC_MAX bounds.
 * Ref: eval/functions/mpfr_set_default_prec/spec.json -- contract.
 */

import { MPFRError, PREC_MIN, PREC_MAX } from "../core.ts";

export function mpfr_set_default_prec(prec: bigint): bigint {
  if (typeof prec !== 'bigint') {
    throw new MPFRError(
      'EPREC',
      `mpfr_set_default_prec: prec must be bigint, got ${typeof prec}`,
    );
  }
  // C: MPFR_ASSERTN(MPFR_PREC_COND(prec)) -> lift to recoverable throw.
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  // Store verbatim; resulting default precision equals prec.
  return prec;
}
