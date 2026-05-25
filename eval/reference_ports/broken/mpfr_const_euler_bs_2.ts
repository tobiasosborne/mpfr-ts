/**
 * reference_ports/broken/mpfr_const_euler_bs_2.ts -- deliberately-buggy.
 *
 * **BUG: in the n1!=0 base case, P = (2n1-1)^2 instead of (2n1-1)^3.**
 * Off-by-power on the cubic. Every n1!=0 base case is wrong by a factor
 * of (2n1-1); every recursive call composes the error.
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
      // BUG: should be cubed; squared is wrong.
      P = twoN1m1 * twoN1m1;
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
  n1: bigint, n2: bigint, N: bigint, cont: number,
): BsTriple {
  if (typeof n1 !== 'bigint' || typeof n2 !== 'bigint' || typeof N !== 'bigint') {
    throw new MPFRError('EDOMAIN', 'mpfr_const_euler_bs_2: bad type');
  }
  if (n1 >= n2 || N < 1n) {
    throw new MPFRError('EDOMAIN', 'mpfr_const_euler_bs_2: bad input');
  }
  return bs2(n1, n2, N, cont);
}
