/**
 * reference_ports/correct/mpfr_const_euler_bs_1.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/const_euler.c L73-L135): binary-splitting
 * recursion. Reproduced verbatim using native BigInt arithmetic.
 *
 * Base case (n2 - n1 == 1):
 *   P = N^2
 *   Q = (n1+1)^2
 *   C = 1
 *   D = n1+1
 *   T = N^2 (= P)
 *   V = N^2 (= P)
 *
 * Recursive case:
 *   m = (n1 + n2) / 2
 *   recurse (n1, m) cont=1 -> L
 *   recurse (m, n2) cont=1 -> R
 *   if cont: P = L.P * R.P
 *   Q = L.Q * R.Q
 *   D = L.D * R.D
 *   T = L.P * R.T + R.Q * L.T
 *   if cont: C = L.C * R.D + R.C * L.D
 *   V = R.D * (R.Q * L.V + L.C * L.P * R.T) + L.D * L.P * R.V
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export interface Bs1Tuple {
  P: bigint;
  Q: bigint;
  T: bigint;
  C: bigint;
  D: bigint;
  V: bigint;
}

function bs1(n1: bigint, n2: bigint, N: bigint, cont: number): Bs1Tuple {
  if (n2 - n1 === 1n) {
    const Nsq = N * N;
    const n1p1 = n1 + 1n;
    return {
      P: Nsq,
      Q: n1p1 * n1p1,
      T: Nsq,
      C: 1n,
      D: n1p1,
      V: Nsq,
    };
  } else {
    const m = (n1 + n2) / 2n;
    const L = bs1(n1, m, N, 1);
    const R = bs1(m, n2, N, 1);
    // C source (const_euler.c L99-L121): when cont=0, P and C are NOT
    // written — they retain the init value of 0. Only the recursive
    // (cont=1) branch multiplies into them.
    const P = cont !== 0 ? L.P * R.P : 0n;
    const Q = L.Q * R.Q;
    const D = L.D * R.D;
    // T = LP RT + RQ LT
    const T_part_LR = L.P * R.T;
    const T = T_part_LR + R.Q * L.T;
    const C = cont !== 0 ? L.C * R.D + R.C * L.D : 0n;
    // V = RD (RQ LV + LC LP RT) + LD LP RV
    const u = L.P * R.V * L.D;
    let v = R.Q * L.V + L.C * T_part_LR;  // L.C * (L.P * R.T)
    v = v * R.D;
    const V = u + v;
    return { P, Q, T, C, D, V };
  }
}

export function mpfr_const_euler_bs_1(
  n1: bigint,
  n2: bigint,
  N: bigint,
  cont: number,
): Bs1Tuple {
  if (typeof n1 !== 'bigint' || typeof n2 !== 'bigint' || typeof N !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_1: n1/n2/N must be bigint`);
  }
  if (typeof cont !== 'number' || !Number.isInteger(cont)) {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_1: cont must be int`);
  }
  if (n1 >= n2) {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_1: requires n1 < n2`);
  }
  if (N < 1n) {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_1: requires N >= 1`);
  }
  return bs1(n1, n2, N, cont);
}
