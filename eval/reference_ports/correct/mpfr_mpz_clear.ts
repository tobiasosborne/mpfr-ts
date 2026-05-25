/**
 * reference_ports/correct/mpfr_mpz_clear.ts -- mutation-prove reference.
 *
 * The C function (mpfr/src/pool.c L83-L101) either pushes the mpz_t
 * back onto the static mpz_tab pool or calls mpz_clear to free the
 * limb storage. TS has no such pool (the JS GC handles bigint
 * lifetimes), so the port is a no-op returning `true` as a success
 * marker -- same convention as mpfr_free_pool (worklog 020).
 *
 * Per ADR 0003, bigint values are the mpz_t analogue; there's nothing
 * to clear at the API surface.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_mpz_clear(z: bigint): boolean {
  if (typeof z !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_clear: z must be bigint`);
  }
  return true;
}
