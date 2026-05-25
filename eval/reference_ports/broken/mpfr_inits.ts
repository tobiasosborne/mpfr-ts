/**
 * reference_ports/broken/mpfr_inits.ts -- deliberately-buggy.
 *
 * **Collapses output to a constant 0n** regardless of input. Every
 * non-zero n fails strict equality; composite well below 0.30.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_inits(n: bigint): bigint {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_inits: bad input`);
  }
  return 0n;
}
