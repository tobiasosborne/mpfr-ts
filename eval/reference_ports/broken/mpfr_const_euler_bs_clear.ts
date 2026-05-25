/**
 * reference_ports/broken/mpfr_const_euler_bs_clear.ts -- deliberately-buggy.
 *
 * **BUG: only zeroes 5 of the 6 fields (forgets V).** Off-by-one on
 * field count -- a classic copy-paste oversight.
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

export function mpfr_const_euler_bs_clear(s: ConstEulerBs): ConstEulerBs {
  // BUG: V should also be 0n.
  return { P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: s.V };
}
