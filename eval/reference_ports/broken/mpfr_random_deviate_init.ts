/**
 * reference_ports/broken/mpfr_random_deviate_init.ts -- deliberately-buggy.
 *
 * **BUG: initializes e to 32 instead of 0.** That makes the state
 * appear pre-populated; downstream consumers would mistake the garbage
 * h field for legitimate random bits.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

export function mpfr_random_deviate_init(): RandomDeviate {
  // BUG: e should be 0n. Setting it to 32n claims 32 bits exist.
  return { e: 32n, h: 0n, f: 0n };
}
