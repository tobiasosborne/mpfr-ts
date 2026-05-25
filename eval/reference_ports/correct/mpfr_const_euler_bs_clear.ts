/**
 * reference_ports/correct/mpfr_const_euler_bs_clear.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/const_euler.c L62-L71): mpz_clear on each field.
 * The TS port returns a fully-zeroed state.
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

export function mpfr_const_euler_bs_clear(_s: ConstEulerBs): ConstEulerBs {
  return { P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n };
}
