/**
 * reference_ports/correct/mpfr_mpz_init2.ts -- mutation-prove reference.
 *
 * The C function (mpfr/src/pool.c L55-L80) initialises an mpz_t to
 * value +0 with a bit-count hint n; either pool-reclaims (if n fits
 * in MPFR_POOL_MAX_SIZE * GMP_NUMB_BITS = 2048) or pre-allocates via
 * mpz_init2. Per ADR 0003, a bigint is the mpz_t analogue; there is
 * no allocator pre-sizing, no pool. The n hint is accepted but ignored.
 *
 * The TS port returns 0n unconditionally; n must be a non-negative bigint.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_mpz_init2(n: bigint): bigint {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_init2: n must be bigint, got ${typeof n}`);
  }
  if (n < 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_init2: n must be >= 0, got ${n}`);
  }
  return 0n;
}
