/**
 * reference_ports/correct/mpfr_inits2.ts -- mutation-prove reference.
 *
 * Count-passthrough port with prec validation. The C function
 * (mpfr/src/inits2.c L40-L64) is a void-returning varargs loop that
 * init2's each handle at the supplied prec; no handles in the
 * immutable TS surface, so the port reduces to a count-passthrough
 * success marker.
 *
 * See spec.json divergence_from_c for the full rationale.
 *
 * Ref: mpfr/src/inits2.c L40-L64 -- C reference.
 * Ref: src/ops/init2.ts -- the underlying single-handle init2 (explicit prec).
 */

import { MPFRError, PREC_MAX, PREC_MIN } from '../../../src/core.ts';

export function mpfr_inits2(prec: bigint, n: bigint): bigint {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_inits2: prec must be bigint`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `mpfr_inits2: prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_inits2: prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (typeof n !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_inits2: n must be bigint`);
  }
  if (n < 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_inits2: n must be >= 0, got ${n}`);
  }
  return n;
}
