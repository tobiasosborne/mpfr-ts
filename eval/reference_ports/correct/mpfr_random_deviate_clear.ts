/**
 * reference_ports/correct/mpfr_random_deviate_clear.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/random_deviate.c L80-L84): mpz_clear(x->f).
 * In TS: BigInt is GC-managed. Returns a fully-zeroed state as the
 * closest immutable analog to 'object freed'.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

export function mpfr_random_deviate_clear(_x: RandomDeviate): RandomDeviate {
  return { e: 0n, h: 0n, f: 0n };
}
