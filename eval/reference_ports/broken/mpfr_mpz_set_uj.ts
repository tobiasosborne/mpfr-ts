/**
 * reference_ports/broken/mpfr_mpz_set_uj.ts -- deliberately-buggy.
 *
 * **Collapses output to 0n regardless of input.** Every n != 0n case
 * fails on strict equality.
 */

import { MPFRError } from '../../../src/core.ts';

const UINT64_MAX = (1n << 64n) - 1n;

export function mpfr_mpz_set_uj(n: bigint): bigint {
  if (typeof n !== 'bigint' || n < 0n || n > UINT64_MAX) {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_set_uj: bad n`);
  }
  return 0n;
}
