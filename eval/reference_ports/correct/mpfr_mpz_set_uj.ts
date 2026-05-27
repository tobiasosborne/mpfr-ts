/**
 * reference_ports/correct/mpfr_mpz_set_uj.ts -- mutation-prove reference.
 *
 * The C function (mpfr/src/pow_uj.c L40-L57, static helper) sets an
 * mpz_t z to the uintmax_t value n, with explicit decomposition into
 * (high << ULONG_BITS) | low to work around mpz_set_ui's unsigned long
 * domain limit.
 *
 * Under ADR 0003 (bigint-as-mpz_t), the TS port is identity for the
 * uint64 domain: bigint has no decomposition need; n -> n.
 *
 * Ref: mpfr/src/pow_uj.c L40-L57 -- C reference (static helper).
 * Ref: docs/adr/0003-mpz-api.md -- bigint as mpz analogue.
 */

import { MPFRError } from '../../../src/core.ts';

/** Upper bound of uintmax_t on x86_64 Linux: 2^64 - 1. */
const UINT64_MAX = (1n << 64n) - 1n;

export function mpfr_mpz_set_uj(n: bigint): bigint {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_set_uj: n must be bigint, got ${typeof n}`);
  }
  if (n < 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_set_uj: n must be >= 0 (uintmax_t domain), got ${n}`);
  }
  if (n > UINT64_MAX) {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_set_uj: n must be <= 2^64 - 1, got ${n}`);
  }
  return n;
}
