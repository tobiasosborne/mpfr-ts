/**
 * reference_ports/correct/mpfr_get_cputime.ts -- mutation-prove reference.
 *
 * The C function (mpfr/src/logging.c L114-L132) reads user-mode CPU
 * time via getrusage; it exists for libmpfr logging instrumentation
 * only. The TS port returns 0 unconditionally. See spec.json divergence_from_c.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_get_cputime(): number {
  return 0;
}
