/**
 * reference_ports/broken/mpfr_mpz_init2.ts -- deliberately-buggy.
 *
 * **Collapses output to n instead of 0n.** Every n != 0n case fails on
 * strict equality.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_mpz_init2(n: bigint): bigint {
  if (typeof n !== 'bigint' || n < 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_init2: bad n`);
  }
  // BUG: return n instead of 0n.
  return n;
}
