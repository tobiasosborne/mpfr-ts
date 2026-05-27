/**
 * port.ts -- pure-TS port of MPFR's `mpfr_nbits_uj`.
 *
 * Computes the number of significant bits of n, i.e. floor(log2(n)) + 1,
 * equivalently the 1-indexed position of the highest set bit.
 *
 * Ref: mpfr/src/nbits_ulong.c L89-L136 -- the C reference body. The C
 *   implementation uses a divide-and-conquer bit search (uintmax_t may be
 *   wider than unsigned long / mp_limb_t, so count_leading_zeros is not
 *   available). The algorithm is byte-identical to mpfr_nbits_ulong on LP64
 *   (both are 64-bit types).
 *
 * Ref: src/ops/nbits_ulong.ts -- the unsigned long sister (same algorithm).
 *
 * Input contract
 * --------------
 * C: `int mpfr_nbits_uj(uintmax_t n)`. TS: `bigint` in [1n, 2^64 - 1n].
 * A JS `number` throws MPFRError('EPREC'). n = 0 throws MPFRError('EDOMAIN')
 * (C asserts on this; we don't model C assertions as process crashes).
 *
 * Output contract
 * ---------------
 * Plain JS `number` in [1, 64] -- matches the C `int` return.
 */

import { MPFRError } from "../core.ts";

/** Upper bound of uintmax_t on LP64: 2^64 - 1. */
const UINTMAX_MAX: bigint = (1n << 64n) - 1n;

/**
 * Number of significant bits of `n`, i.e. `floor(log2(n)) + 1`.
 *
 * @param n - A `bigint` with `0 < n < 2^64`.
 * @returns The bit length of `n` as a `number` in `[1, 64]`.
 * @throws {MPFRError} `EPREC` if `n` is not a `bigint`, or if `n >= 2^64`.
 * @throws {MPFRError} `EDOMAIN` if `n <= 0n` (C asserts on this).
 */
export function mpfr_nbits_uj(n: bigint): number {
  if (typeof n !== 'bigint') {
    throw new MPFRError(
      'EPREC',
      `mpfr_nbits_uj: n must be bigint, got ${typeof n}`,
    );
  }
  if (n <= 0n) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_nbits_uj: n must be > 0, got ${n}`,
    );
  }
  if (n > UINTMAX_MAX) {
    throw new MPFRError(
      'EPREC',
      `mpfr_nbits_uj: n must fit in uintmax_t (< 2^64), got ${n}`,
    );
  }

  // Divide-and-conquer bit search: collapse the search window in
  // five stages of 16, 8, 4, 2, 1 bits. Mirrors the C #else branch
  // (mpfr/src/nbits_ulong.c L94-L131) verbatim in structure.
  let cnt = 0;
  let v = n;

  while (v >= 0x10000n) {
    v >>= 16n;
    cnt += 16;
  }
  if (v >= 0x100n) {
    v >>= 8n;
    cnt += 8;
  }
  if (v >= 0x10n) {
    v >>= 4n;
    cnt += 4;
  }
  if (v >= 4n) {
    v >>= 2n;
    cnt += 2;
  }
  // v is now 1, 2, or 3. cnt += 1 + (v >= 2).
  cnt += 1 + (v >= 2n ? 1 : 0);
  return cnt;
}
