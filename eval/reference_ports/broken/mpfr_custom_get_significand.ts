/**
 * reference_ports/broken/mpfr_custom_get_significand.ts -- deliberately-buggy.
 *
 * **BUG: returns the mantissa shifted right by 1 bit (`x.mant >> 1n`).**
 * Plausible mistake: agent loses the MSB-alignment property thinking
 * the mantissa needs to be "normalised" again. Every nonzero case has
 * an off-by-2 output.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_custom_get_significand(x: MPFR): bigint {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'mpfr_custom_get_significand: x must be MPFR');
  }
  if (x.kind !== 'normal') {
    throw new MPFRError('EDOMAIN', `mpfr_custom_get_significand: singular`);
  }
  // BUG: should return x.mant. The right-shift destroys MSB alignment.
  return x.mant >> 1n;
}
