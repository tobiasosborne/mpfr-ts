/**
 * reference_ports/broken/mpfr_custom_init.ts -- deliberately-buggy.
 *
 * **BUG: returns negZero instead of posZero.** Off-by-sign. Every case
 * mismatches on the sign field.
 */

import type { MPFR } from '../../../src/core.ts';
import { negZero } from '../../../src/core.ts';

export function mpfr_custom_init(prec: bigint): MPFR {
  // BUG: should be posZero(prec). The sign mismatch fails every case.
  return negZero(prec);
}
