/**
 * reference_ports/broken/mpfr_nbits_uj.ts -- deliberately-buggy.
 *
 * **Collapses output to a constant 1** regardless of input. Every
 * n > 1 case fails on strict equality (the correct answer ranges
 * over [1, 64]); composite well below 0.30.
 */

import { MPFRError } from '../../../src/core.ts';

const UINTMAX_MAX: bigint = (1n << 64n) - 1n;

export function mpfr_nbits_uj(n: bigint): number {
  if (typeof n !== 'bigint' || n <= 0n || n > UINTMAX_MAX) {
    throw new MPFRError('EDOMAIN', `mpfr_nbits_uj: bad n`);
  }
  // BUG: constant output.
  return 1;
}
