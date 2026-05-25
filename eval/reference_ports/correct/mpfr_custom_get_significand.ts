/**
 * reference_ports/correct/mpfr_custom_get_significand.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/stack_interface.c L39-L44):
 *   return (void*) MPFR_MANT(x)
 *
 * The TS schema stores the mantissa as a single MSB-aligned bigint on
 * MPFR.mant. Return it directly.
 *
 * Singular inputs are out of scope (the C function returns a meaningless
 * pointer; the TS port throws to fail-fast).
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_custom_get_significand(x: MPFR): bigint {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'mpfr_custom_get_significand: x must be MPFR');
  }
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_custom_get_significand: singular inputs out of scope (kind=${x.kind})`,
    );
  }
  return x.mant;
}
