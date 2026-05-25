/**
 * reference_ports/broken/mpfr_clear_flags.ts -- deliberately-buggy.
 *
 * **BUG: returns the input mask UNCHANGED instead of 0n.** The most
 * basic mistake -- treating the function as a no-op.
 *
 * Expected gap: correct=1.0, broken<0.55. Only mask=0n cases pass.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_clear_flags(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_flags: mask must be bigint, got ${typeof mask}`,
    );
  }
  // BUG: should return 0n; returning the input means flags are never cleared.
  return mask & 63n;
}
