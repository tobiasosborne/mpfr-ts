/**
 * reference_ports/broken/mpfr_random_deviate_reset.ts -- deliberately-buggy.
 *
 * **BUG: returns x unchanged (no reset).** Forgets to zero e.
 * Every case with e != 0 mismatches.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

export function mpfr_random_deviate_reset(x: RandomDeviate): RandomDeviate {
  // BUG: should zero e. Returning x unchanged keeps the bit-counter alive.
  return { e: x.e, h: x.h, f: x.f };
}
