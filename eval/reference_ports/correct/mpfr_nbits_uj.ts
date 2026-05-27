/**
 * reference_ports/correct/mpfr_nbits_uj.ts -- mutation-prove reference.
 *
 * Number of significant bits of n, i.e. floor(log2(n)) + 1. Mirrors
 * mpfr/src/nbits_ulong.c L89-L136 (the uintmax_t sister of
 * mpfr_nbits_ulong), which uses a divide-and-conquer bit search since
 * the C side cannot count on count_leading_zeros for uintmax_t when
 * uintmax_t exceeds a single GMP limb.
 *
 * On LP64 the algorithm is byte-identical to src/ops/nbits_ulong.ts;
 * we delegate to that port's algorithm for consistency.
 *
 * Ref: mpfr/src/nbits_ulong.c L89-L136 -- C reference body.
 * Ref: src/ops/nbits_ulong.ts -- the unsigned long sister (same algo).
 */

import { MPFRError } from '../../../src/core.ts';

/** Upper bound of uintmax_t on LP64 (the platforms we care about): 2^64 - 1. */
const UINTMAX_MAX: bigint = (1n << 64n) - 1n;

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

  // Divide-and-conquer bit search; mirrors mpfr/src/nbits_ulong.c L94-L131.
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
  // v is now 1, 2, or 3.
  cnt += 1 + (v >= 2n ? 1 : 0);
  return cnt;
}
