/**
 * reference_ports/broken/mpfr_const_euler_bs_init.ts -- deliberately-buggy.
 *
 * **BUG: initialises P, Q, T, C, D, V to 1n instead of 0n.** Subtly
 * wrong: mpz_init creates a *zero*-valued mpz; 1n is wrong.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export interface ConstEulerBs {
  P: bigint;
  Q: bigint;
  T: bigint;
  C: bigint;
  D: bigint;
  V: bigint;
}

export function mpfr_const_euler_bs_init(): ConstEulerBs {
  // BUG: mpz_init creates 0, not 1.
  return { P: 1n, Q: 1n, T: 1n, C: 1n, D: 1n, V: 1n };
}
