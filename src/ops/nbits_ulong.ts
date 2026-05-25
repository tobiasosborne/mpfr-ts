/**
 * ops/nbits_ulong.ts -- pure-TS port of MPFR's `mpfr_nbits_ulong`.
 *
 * Compute the number of significant bits of `n`, i.e.
 * `floor(log2(n)) + 1` for `n >= 1` -- equivalently the 1-indexed
 * position of the highest set bit. The C body returns
 * `nbits(unsigned long) - count_leading_zeros(n)`; on x86_64 LP64
 * the platform unsigned long is 64 bits, so the result lies in
 * `[1, 64]`.
 *
 * Ref: mpfr/src/nbits_ulong.c L29-L84 -- C reference. The C source
 *   has two implementations keyed on `MPFR_LONG_WITHIN_LIMB`: the
 *   `#ifdef` branch (L36-L39) uses the GMP `count_leading_zeros`
 *   macro; the `#else` branch (L41-L78) is a divide-and-conquer bit
 *   search. We mirror the `#else` branch because BigInt has no
 *   native CLZ in TypeScript; comparisons against power-of-two
 *   BigInt constants are O(1) in V8 / JSC, so this is plenty tight
 *   for the misc-class 1000ms per-case budget.
 *
 * ## Input contract
 *
 * The C signature is `int mpfr_nbits_ulong(unsigned long n)`. TypeScript
 * has no `unsigned long`; we accept a `bigint` and validate
 * `0 < n < 2^64` to match the LP64 platform width libmpfr targets.
 * A JS `number` input is rejected (`MPFRError('EPREC', ...)`) -- the
 * caller should `BigInt(n)` at the call site. The harness's
 * `golden_driver` emits `n` as a u64 decimal string via
 * `jl_kv_u64`, which the value codec decodes to a `bigint`, so the
 * wire shape matches without further adaptation.
 *
 * ## Output contract
 *
 * Plain JS `number` in `[1, 64]` -- matches the C `int` return and
 * trivially fits a `number`. The wire format is a bare JSON number
 * via `jl_output_scalar_int`.
 *
 * ## Divergences from C (per spec.json)
 *
 *   1. `n = 0` -- C asserts (`MPFR_ASSERTD(n > 0)`); TS throws
 *      `MPFRError('EDOMAIN', ...)`. We don't model C assertions as
 *      process crashes -- a thrown exception is the recoverable
 *      idiom that keeps the harness worker pool healthy. The
 *      golden_driver excludes `n = 0` by construction.
 *   2. Input type -- C takes `unsigned long`; TS takes `bigint`.
 *      A `number`-shaped input throws `MPFRError('EPREC', ...)`.
 *   3. `n >= 2^64` -- outside the C platform domain on LP64; we
 *      reject with `MPFRError('EPREC', ...)` rather than silently
 *      truncate.
 *
 * Ref: eval/functions/mpfr_nbits_ulong/spec.json -- full contract.
 * Ref: src/core.ts L197-L209 -- `MPFRError` class.
 *
 * @mpfrName mpfr_nbits_ulong
 */

import { MPFRError } from '../core.ts';

/** Largest representable `unsigned long` on LP64: `2^64 - 1`. */
const U64_MAX: bigint = (1n << 64n) - 1n;

/**
 * Number of significant bits of `n`, i.e. `floor(log2(n)) + 1`.
 *
 * @param n - A `bigint` with `0 < n < 2^64`.
 * @returns The bit length of `n` as a `number` in `[1, 64]`.
 * @throws {MPFRError} `EPREC` if `n` is not a `bigint`, or if
 *                    `n >= 2^64`.
 * @throws {MPFRError} `EDOMAIN` if `n <= 0n` (C asserts on this).
 *
 * @mpfrName mpfr_nbits_ulong
 */
export function mpfr_nbits_ulong(n: bigint): number {
  if (typeof n !== 'bigint') {
    throw new MPFRError(
      'EPREC',
      `mpfr_nbits_ulong: n must be bigint, got ${typeof n}`,
    );
  }
  if (n <= 0n) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_nbits_ulong: n must be > 0, got ${n}`,
    );
  }
  if (n > U64_MAX) {
    throw new MPFRError(
      'EPREC',
      `mpfr_nbits_ulong: n must fit in unsigned long (< 2^64), got ${n}`,
    );
  }

  // Divide-and-conquer bit search: collapse the search window in
  // five stages of 16, 8, 4, 2, 1 bits. Mirrors the C #else branch
  // (mpfr/src/nbits_ulong.c L42-L78) verbatim in structure.
  let cnt = 0;
  let v = n;

  while (v >= 0x10000n) {
    v >>= 16n;
    cnt += 16;
  }
  // v <= 0xffff
  if (v >= 0x100n) {
    v >>= 8n;
    cnt += 8;
  }
  // v <= 0xff
  if (v >= 0x10n) {
    v >>= 4n;
    cnt += 4;
  }
  // v <= 0xf
  if (v >= 4n) {
    v >>= 2n;
    cnt += 2;
  }
  // v is now 1, 2, or 3. cnt += 1 + (v >= 2).
  cnt += 1 + (v >= 2n ? 1 : 0);
  return cnt;
}
