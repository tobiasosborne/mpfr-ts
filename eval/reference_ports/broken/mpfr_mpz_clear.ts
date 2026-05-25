/**
 * reference_ports/broken/mpfr_mpz_clear.ts -- deliberately-buggy.
 *
 * **Collapses output to false instead of true.** Every case fails on
 * strict equality.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_mpz_clear(z: bigint): boolean {
  if (typeof z !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_clear: bad input`);
  }
  return false;
}
