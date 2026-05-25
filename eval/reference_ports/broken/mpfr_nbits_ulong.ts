/**
 * reference_ports/broken/mpfr_nbits_ulong.ts -- deliberately-buggy
 * mpfr_nbits_ulong.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_nbits_ulong golden, the golden is too weak.
 *
 * **Deliberately broken: off-by-one at the final "n in {1, 2, 3}" branch.**
 * The C source returns `1 + (n >= 2)` for the terminal trio:
 *   n=1 -> 1, n=2 -> 2, n=3 -> 2.
 * The broken port returns `(n >= 2)`, dropping the constant 1:
 *   n=1 -> 0, n=2 -> 1, n=3 -> 1.
 * Combined with the prefix accumulators, every input ends up with cnt
 * one less than the correct value -- a systematic off-by-one.
 *
 * Why this bug shape: the trio of statements at mpfr/src/nbits_ulong.c
 * L77-L78 is the most common place for an agent to fumble the
 * +1-vs-no-+1 distinction, especially because earlier branches add
 * powers-of-two-bit counts (16, 8, 4, 2) and the +1 here looks like
 * "just another constant" rather than the load-bearing "we still owe
 * one bit for n itself" addition.
 *
 * Expected failure surface:
 *   - happy: every case (20/20) fails -- output is off by 1.
 *   - edge: every case (30/30) fails.
 *   - adversarial: every case (10/10) fails.
 *   - fuzz: every case (50/50) fails.
 *   Total: 110/110 fail -- composite ~ 0.0. Well outside danger zone.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 -- mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 -- composite must drop below 0.55 under mutation.
 * Ref: eval/reference_ports/correct/mpfr_nbits_ulong.ts -- correct version.
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
    throw new MPFRError('EPREC', `mpfr_nbits_ulong: n must fit in unsigned long, got ${n}`);
  }

  let cnt = 0;
  let v = n;

  while (v >= 0x10000n) { v >>= 16n; cnt += 16; }
  if (v >= 0x100n)      { v >>= 8n;  cnt += 8;  }
  if (v >= 0x10n)       { v >>= 4n;  cnt += 4;  }
  if (v >= 4n)          { v >>= 2n;  cnt += 2;  }

  // BUG: should be `cnt += 1 + (v >= 2n ? 1 : 0)`. The constant +1
  // accounts for n itself; dropping it makes every output off by one.
  cnt += (v >= 2n ? 1 : 0);
  return cnt;
}
