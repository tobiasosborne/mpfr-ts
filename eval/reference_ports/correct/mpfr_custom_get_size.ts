/**
 * reference_ports/correct/mpfr_custom_get_size.ts -- mutation-prove
 * reference for mpfr_custom_get_size.
 *
 * Algorithm (mpfr/src/stack_interface.c L25-L30):
 *   bytes = MPFR_PREC2LIMBS(prec) * MPFR_BYTES_PER_MP_LIMB
 *         = ceil(prec / 64) * 8     (on LP64)
 *
 * The TS port returns a bigint (unbounded range) computed via the same
 * formula. Validates prec via PREC_MIN/PREC_MAX from src/core.ts.
 *
 * Ref: mpfr/src/stack_interface.c L25-L30 -- C reference.
 * Ref: eval/functions/mpfr_custom_get_size/spec.json -- contract.
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
  if (prec < PREC_MIN) {
    throw new MPFRError(
      'EPREC',
      `mpfr_custom_get_size: prec must be >= ${PREC_MIN}, got ${prec}`,
    );
  }
  if (prec > PREC_MAX) {
    throw new MPFRError(
      'EPREC',
      `mpfr_custom_get_size: prec must be <= ${PREC_MAX}, got ${prec}`,
    );
  }
  // ceil(prec / 64) * 8 -- mirrors MPFR_PREC2LIMBS macro on LP64.
  const limbs = (prec + GMP_NUMB_BITS - 1n) / GMP_NUMB_BITS;
  return limbs * BYTES_PER_LIMB;
}
