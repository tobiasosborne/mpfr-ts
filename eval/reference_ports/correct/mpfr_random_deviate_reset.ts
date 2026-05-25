/**
 * reference_ports/correct/mpfr_random_deviate_reset.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/random_deviate.c L73-L77): x->e = 0;
 * The TS port returns {e: 0n, h: x.h, f: x.f} -- preserves h and f
 * verbatim (deterministic wire shape for grader stability).
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

export function mpfr_random_deviate_reset(x: RandomDeviate): RandomDeviate {
  return { e: 0n, h: x.h, f: x.f };
}
