/**
 * mpfr_mpz_set_uj -- pure-TS port of MPFR's static helper.
 *
 * C: static void mpfr_mpz_set_uj(mpz_t z, uintmax_t n).
 *    Ref: mpfr/src/pow_uj.c L40-L57 -- the C body decomposes n into
 *    (high << ULONG_BITS) | low because mpz_set_ui only takes unsigned
 *    long. Under ADR 0003 (bigint-as-mpz_t), TS bigint is arbitrary-
 *    precision natively; the operation reduces to n -> n for the uint64
 *    domain. We validate the domain and return n unchanged.
 *
 * TS: mpfr_mpz_set_uj(n: bigint) -> bigint.
 *
 * @param n  The unsigned integer value, as a bigint. Must be in
 *           [0n, UINT64_MAX] (the uint64 domain that uintmax_t occupies
 *           on x86_64 Linux).
 * @returns  `n` unchanged.
 *
 * @throws {MPFRError} `EDOMAIN` if `n < 0n` or `n > UINT64_MAX`, or if
 *                     `n` is not a bigint.
 *
 * Refs
 * ----
 *   - mpfr/src/pow_uj.c L40-L57 -- C reference (static helper).
 *   - docs/adr/0003-mpz-api.md -- bigint-as-mpz_t.
 *   - src/ops/set_uj_2exp.ts -- reference for uintmax_t domain validation
 *     (UINT64_MAX constant).
 */

import { MPFRError } from "../core.ts";

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
