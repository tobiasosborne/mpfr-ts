/**
 * reference_ports/broken/mpfr_random_deviate_clear.ts -- deliberately-buggy.
 *
 * **BUG: only zeroes f, leaves e and h.** Forgets the full clear.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

export function mpfr_random_deviate_clear(x: RandomDeviate): RandomDeviate {
  // BUG: should zero all three. Only zeros f.
  return { e: x.e, h: x.h, f: 0n };
}
