/**
 * reference_ports/correct/mpfr_inits.ts -- mutation-prove reference.
 *
 * Count-passthrough port: takes a bigint n, returns n unchanged. The
 * C function (mpfr/src/inits.c L37-L60) is a void-returning varargs
 * loop that init's each handle at default prec (53 bits); there are no
 * handles in the immutable TS surface, so the port reduces to a
 * count-passthrough success marker.
 *
 * See spec.json divergence_from_c for the full rationale.
 *
 * Ref: mpfr/src/inits.c L37-L60 -- C reference.
 * Ref: src/ops/init.ts -- the underlying single-handle init (default prec).
 */

import { MPFRError } from '../core.ts';

export function mpfr_inits(n: bigint): bigint {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_inits: n must be bigint`);
  }
  if (n < 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_inits: n must be >= 0, got ${n}`);
  }
  return n;
}
