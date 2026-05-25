/**
 * reference_ports/correct/mpfr_nbits_ulong.ts -- mutation-prove reference
 * for mpfr_nbits_ulong.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline. The production src/ops/nbits_ulong.ts does not
 * yet exist; the orchestrator will materialise it during the port-and-
 * grade flow. This file mirrors the C divide-and-conquer search from
 * mpfr/src/nbits_ulong.c L42-L78 (the #else branch active when
 * MPFR_LONG_WITHIN_LIMB is false; we use the same shape regardless
 * because BigInt has no native count_leading_zeros).
 *
 * Algorithm (mpfr/src/nbits_ulong.c L42-L78):
 *   cnt = 0;
 *   while n >= 0x10000: n >>= 16; cnt += 16;
 *   if n >= 0x100:      n >>= 8;  cnt += 8;
 *   if n >= 0x10:       n >>= 4;  cnt += 4;
 *   if n >= 4:          n >>= 2;  cnt += 2;
 *   // n in {1, 2, 3}
 *   cnt += 1 + (n >= 2);
 *   return cnt;  // == floor(log2(original_n)) + 1
 *
 * Type contract:
 *   - Input is `bigint`; the C `unsigned long` is 64-bit on the
 *     platforms libmpfr targets (LP64), so we accept 0 < n < 2^64.
 *   - Output is `number` (always in [1, 64]).
 *   - n = 0 throws MPFRError('EDOMAIN') per spec.json divergence_from_c.
 *
 * Ref: mpfr/src/nbits_ulong.c L29-L84 -- C reference.
 * Ref: eval/functions/mpfr_nbits_ulong/spec.json -- contract.
 */

import { MPFRError } from '../../../src/core.ts';

const U64_MAX: bigint = (1n << 64n) - 1n;

export function mpfr_nbits_ulong(n: bigint): number {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_nbits_ulong: n must be bigint, got ${typeof n}`);
  }
  if (n <= 0n) {
    throw new MPFRError('EDOMAIN', `mpfr_nbits_ulong: n must be > 0, got ${n}`);
  }
  if (n > U64_MAX) {
    throw new MPFRError('EPREC', `mpfr_nbits_ulong: n must fit in unsigned long (< 2^64), got ${n}`);
  }

  let cnt = 0;
  let v = n;

  // Halve the search window in 5 stages: 16, 8, 4, 2, 1 bits.
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
