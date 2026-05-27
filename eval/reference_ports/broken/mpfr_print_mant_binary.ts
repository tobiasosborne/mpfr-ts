/**
 * reference_ports/broken/mpfr_print_mant_binary.ts -- deliberately-buggy.
 *
 * **Collapses output to a constant 'broken\n' regardless of input.**
 * Every case fails on strict equality; composite well below 0.30.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_print_mant_binary(str: string, x: MPFR): string {
  if (typeof str !== 'string') {
    throw new MPFRError('EDOMAIN', `mpfr_print_mant_binary: bad str`);
  }
  if (x === null || typeof x !== 'object' || x.kind !== 'normal') {
    throw new MPFRError('EDOMAIN', `mpfr_print_mant_binary: bad x`);
  }
  // BUG: constant output.
  return 'broken\n';
}
