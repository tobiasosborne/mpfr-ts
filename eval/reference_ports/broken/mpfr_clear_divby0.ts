/**
 * reference_ports/broken/mpfr_clear_divby0.ts -- deliberately-buggy
 * mpfr_clear_divby0.
 *
 * **Deliberately broken: clears MPFR_FLAGS_OVERFLOW (bit 2) instead of
 * MPFR_FLAGS_DIVBY0 (bit 32).** Common bug: off-by-position on the bit
 * constant. Plausible if a porter copy-pastes the clear_overflow body
 * without changing the bit name. The output differs from expected on
 * every case where the OVERFLOW and DIVBY0 bits differ in `mask` -- the
 * vast majority of cases.
 *
 * Expected gap: correct=1.0, broken<0.55. With ~6-bit input domain and
 * an inverted mask on the wrong bit, only the cases where `mask`
 * happens to have OVERFLOW and DIVBY0 both clear, or both set, will
 * pass -- roughly 1/4 of cases at most.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_OVERFLOW = 2n;  /* BUG: should be DIVBY0 (32) */

export function mpfr_clear_divby0(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_divby0: mask must be bigint, got ${typeof mask}`,
    );
  }
  const inDomain = mask & MPFR_FLAGS_ALL;
  // BUG: clears the wrong bit (OVERFLOW=2 instead of DIVBY0=32).
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_OVERFLOW);
}
