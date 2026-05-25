/**
 * reference_ports/broken/mpfr_custom_get_size.ts -- deliberately-buggy.
 *
 * **BUG: uses floor division instead of ceil.** Returns
 * `(prec / 64) * 8` rather than `ceil(prec / 64) * 8`. Mismatches every
 * case where prec is not a multiple of 64 -- which is the majority of
 * the test surface.
 *
 * Expected gap: correct=1.0, broken<0.55. Only multiples-of-64 cases pass.
 */

import { MPFRError, PREC_MIN, PREC_MAX } from '../../../src/core.ts';

const GMP_NUMB_BITS = 64n;
const BYTES_PER_LIMB = 8n;

export function mpfr_custom_get_size(prec: bigint): bigint {
  if (typeof prec !== 'bigint') {
    throw new MPFRError(
      'EPREC',
      `mpfr_custom_get_size: prec must be bigint, got ${typeof prec}`,
    );
  }
  if (prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_custom_get_size: prec out of range, got ${prec}`);
  }
  // BUG: floor division instead of ceil (missing the `+ 63` rounding).
  const limbs = prec / GMP_NUMB_BITS;
  return limbs * BYTES_PER_LIMB;
}
