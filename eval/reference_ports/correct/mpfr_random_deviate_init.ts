/**
 * reference_ports/correct/mpfr_random_deviate_init.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/random_deviate.c L65-L70):
 *   mpz_init(x->f); x->e = 0;
 *
 * The TS port returns a fresh state object. Since h and f are explicitly
 * undefined when e=0 (per the C struct contract), the canonical
 * representation uses 0n for all three fields.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

export function mpfr_random_deviate_init(): RandomDeviate {
  return { e: 0n, h: 0n, f: 0n };
}
