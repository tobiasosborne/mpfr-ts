/**
 * reference_ports/broken/mpfr_custom_move.ts -- deliberately-buggy.
 *
 * **BUG: negates the sign on the way through.** Trivially wrong --
 * the function should be the identity; negating the sign mismatches
 * every case where x.sign != 0 (i.e., everything except NaN).
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_custom_move(x: MPFR): MPFR {
  // BUG: should return x unchanged. Sign flip wrecks every test.
  if (x.kind === 'nan') return x;
  return { ...x, sign: (x.sign === 1 ? -1 : 1) as 1 | -1 };
}
