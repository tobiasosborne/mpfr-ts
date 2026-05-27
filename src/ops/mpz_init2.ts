/**
 * mpfr_mpz_init2 -- pure-TS port of MPFR's mpfr_mpz_init2.
 *
 * Class: misc.
 *
 * C signature (mpfr/src/pool.c L55-L80):
 *
 *   void mpfr_mpz_init2 (mpz_ptr z, mp_bitcnt_t n)
 *
 * In C, the function either reclaims an mpz_t from the static mpz_tab pool
 * (if n_alloc > 0 AND n fits within MPFR_POOL_MAX_SIZE * GMP_NUMB_BITS =
 * 32 * 64 = 2048 bits) or calls the real mpz_init2 to pre-allocate at least
 * n bits of limb storage; in both cases the SIZ is set to 0 so the value is
 * +0.
 *
 * The TS port is a vacuous factory taking a bit-count hint n (ignored) and
 * returning 0n. Per ADR 0003 (bigint-as-mpz), there is no allocator, no
 * pool, no explicit init: a bigint is JS-runtime managed. The n parameter is
 * accepted to match the C contract surface but discarded -- JS bigints have
 * no pre-allocation knob, growth is amortised by the runtime.
 *
 * Divergences from C (per spec.json):
 *
 *   1. Return: C void (mutates z in place) -> TS returns 0n (the value of a
 *      freshly-initialised bigint mpz analogue).
 *   2. Parameter: C mp_bitcnt_t n is a pre-allocation HINT; TS accepts a
 *      bigint n >= 0 but DISCARD it.
 *   3. Pool: C reclaims from a static thread-local pool when n fits the
 *      per-entry cap (2048 bits); TS has no pool.
 *   4. Domain: C signature is mp_bitcnt_t (unsigned long, [0, 2^64-1]); TS
 *      accepts bigint n >= 0n. Negative n throws MPFRError('EDOMAIN').
 *
 * Refs:
 *   - mpfr/src/pool.c L55-L80 -- the C reference body.
 *   - docs/adr/0003-mpz-api.md -- bigint-as-mpz_t; no pool, no allocator.
 *   - HANDOFF.md gotcha #9 (worklog 020) -- vacuous-with-hint pattern.
 *   - eval/functions/mpfr_mpz_init/spec.json -- the no-arg sister.
 *   - eval/functions/mpfr_mpz_clear/spec.json -- the vacuous teardown.
 *   - eval/functions/mpfr_inits2/spec.json -- precedent (count+prec
 *     passthrough for vacuous init family).
 */

import { MPFRError } from '../core.ts';

/**
 * Initialise an mpz_t (bigint) to +0 with a pre-allocation hint n.
 *
 * The n parameter is a bit-count hint accepted for C-signature compatibility
 * but discarded -- JS bigints have no pre-allocation API.
 *
 * @param n  Bit-count hint (non-negative bigint). Ignored.
 * @returns  0n (the value of a freshly-initialised bigint mpz analogue).
 *
 * @throws {MPFRError} `EDOMAIN` if n < 0n or n is not a bigint.
 */
export function mpfr_mpz_init2(n: bigint): bigint {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_init2: n must be bigint, got ${typeof n}`);
  }
  if (n < 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_mpz_init2: n must be >= 0, got ${n}`);
  }
  return 0n;
}
