/**
 * reference_ports/correct/mpfr_free_pool.ts -- mutation-prove reference.
 *
 * The C function (mpfr/src/pool.c L105-L118) frees the static mpz_tab
 * pool. TS has no such pool (the runtime GC handles bigint lifecycles),
 * so the port is a no-op returning `true` as a success marker.
 * (Codec doesn't natively handle null/undefined scalar outputs; boolean
 * is the established no-op marker — see buildopt_*_p precedent.)
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_free_pool(): boolean {
  return true;
}
