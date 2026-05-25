/**
 * reference_ports/correct/mpfr_clear_flags.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/exceptions.c L144-L150):
 *   __gmpfr_flags = 0
 *
 * Always returns 0n regardless of input mask. The mask is preserved for
 * wire-form consistency with the sister mpfr_clear_<flag> ports but
 * does not affect the output.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_clear_flags(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_flags: mask must be bigint, got ${typeof mask}`,
    );
  }
  // C: __gmpfr_flags = 0. Output is constant 0n regardless of input.
  return 0n;
}
