/**
 * reference_ports/correct/mpfr_const_euler_bs_2.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/const_euler.c L138-L180): binary-splitting
 * recursion. Reproduced verbatim using native BigInt arithmetic.
 *
 * Base case (n2 - n1 == 1):
 *   if n1 == 0: P = 1, Q = 4N
 *   else:       P = (2n1-1)^3, Q = 32 * n1 * N^2
 *   T = P
 *
 * Recursive case:
 *   m = (n1 + n2) / 2
 *   recurse (n1, m) with cont=1 -> (P, Q, T)
 *   recurse (m, n2) with cont=1 -> (P2, Q2, T2)
 *   T = T * Q2 + T2 * P
 *   if cont: P = P * P2
 *   Q = Q * Q2
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export interface BsTriple {
  P: bigint;
  Q: bigint;
  T: bigint;
}

function bs2(n1: bigint, n2: bigint, N: bigint, cont: number): BsTriple {
  if (n2 - n1 === 1n) {
    let P: bigint;
    let Q: bigint;
    if (n1 === 0n) {
      P = 1n;
      Q = 4n * N;
    } else {
      const twoN1m1 = 2n * n1 - 1n;
      P = twoN1m1 * twoN1m1 * twoN1m1;  // (2n1 - 1)^3
      Q = 32n * n1 * N * N;
    }
    const T = P;
    return { P, Q, T };
  } else {
    const m = (n1 + n2) / 2n;
    const left = bs2(n1, m, N, 1);
    const right = bs2(m, n2, N, 1);
    const T = left.T * right.Q + right.T * left.P;
    const P = cont !== 0 ? left.P * right.P : left.P;
    const Q = left.Q * right.Q;
    return { P, Q, T };
  }
}

export function mpfr_const_euler_bs_2(
  n1: bigint,
  n2: bigint,
  N: bigint,
  cont: number,
): BsTriple {
  if (typeof n1 !== 'bigint' || typeof n2 !== 'bigint' || typeof N !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_2: n1/n2/N must be bigint`);
  }
  if (typeof cont !== 'number' || !Number.isInteger(cont)) {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_2: cont must be int`);
  }
  if (n1 >= n2) {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_2: requires n1 < n2`);
  }
  if (N < 1n) {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_2: requires N >= 1`);
  }
  return bs2(n1, n2, N, cont);
}
