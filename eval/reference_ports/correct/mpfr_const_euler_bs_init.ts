/**
 * reference_ports/correct/mpfr_const_euler_bs_init.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/const_euler.c L51-L60): mpz_init on each of the
 * 6 fields. The TS equivalent is creating a fresh object with 6 zero
 * bigints.
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
  return { P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n };
}
