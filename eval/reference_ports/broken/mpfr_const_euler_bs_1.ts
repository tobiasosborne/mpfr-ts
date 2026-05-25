/**
 * reference_ports/broken/mpfr_const_euler_bs_1.ts -- deliberately-buggy.
 *
 * **BUG: base case sets Q = (n1+1) instead of (n1+1)^2.** Missing the
 * square. Every base case is wrong; every recursive call composes the
 * error through L.Q * R.Q. Composite expected well below 0.30.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export interface Bs1Tuple {
  P: bigint; Q: bigint; T: bigint; C: bigint; D: bigint; V: bigint;
}

function bs1(n1: bigint, n2: bigint, N: bigint, cont: number): Bs1Tuple {
  if (n2 - n1 === 1n) {
    const Nsq = N * N;
    const n1p1 = n1 + 1n;
    return {
      P: Nsq,
      // BUG: should be n1p1 * n1p1
      Q: n1p1,
      T: Nsq,
      C: 1n,
      D: n1p1,
      V: Nsq,
    };
  } else {
    const m = (n1 + n2) / 2n;
    const L = bs1(n1, m, N, 1);
    const R = bs1(m, n2, N, 1);
    const P = cont !== 0 ? L.P * R.P : L.P;
    const Q = L.Q * R.Q;
    const D = L.D * R.D;
    const T_part_LR = L.P * R.T;
    const T = T_part_LR + R.Q * L.T;
    const C = cont !== 0 ? L.C * R.D + R.C * L.D : L.C;
    const u = L.P * R.V * L.D;
    let v = R.Q * L.V + L.C * T_part_LR;
    v = v * R.D;
    const V = u + v;
    return { P, Q, T, C, D, V };
  }
}

export function mpfr_const_euler_bs_1(
  n1: bigint, n2: bigint, N: bigint, cont: number,
): Bs1Tuple {
  if (typeof n1 !== 'bigint' || typeof n2 !== 'bigint' || typeof N !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_1: bad type`);
  }
  if (n1 >= n2 || N < 1n) {
    throw new MPFRError('EDOMAIN', `mpfr_const_euler_bs_1: bad input`);
  }
  return bs1(n1, n2, N, cont);
}
