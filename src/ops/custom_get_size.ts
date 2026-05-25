/**
 * ops/custom_get_size.ts -- pure-TS port of MPFR's `mpfr_custom_get_size`.
 *
 * Returns the byte count needed to store a mantissa of `prec` bits in
 * the C limb-array representation. Computed via MPFR's own formula:
 *
 *   MPFR_PREC2LIMBS(prec) * MPFR_BYTES_PER_MP_LIMB
 *     = ceil(prec / GMP_NUMB_BITS) * 8        (on LP64; GMP_NUMB_BITS=64)
 *     = ((prec + 63) / 64) * 8
 *
 * The TS substrate stores the mantissa as a single bigint, so this
 * byte-size has no operational meaning inside the port -- but it is part
 * of the public ABI for consumers laying out their own storage. We
 * compute the LP64 answer because that is the platform the project
 * targets (CLAUDE.md Rule 12, x86_64/aarch64 Bun + Node).
 *
 * Ref: mpfr/src/stack_interface.c L25-L30 -- C reference body.
 * Ref: src/core.ts -- PREC_MIN/PREC_MAX bounds.
 */

import { MPFRError, PREC_MIN, PREC_MAX } from '../core.ts';

const GMP_NUMB_BITS = 64n;
const BYTES_PER_LIMB = 8n;

/**
 * Byte count for an LP64 mantissa of `prec` bits.
 *
 * @mpfrName mpfr_custom_get_size
 *
 * @param prec  Precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @returns     `ceil(prec / 64) * 8` as a bigint.
 */
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
  // MPFR_PREC2LIMBS macro on LP64: (prec + 63) / 64 limbs, 8 bytes each.
  const limbs = (prec + GMP_NUMB_BITS - 1n) / GMP_NUMB_BITS;
  return limbs * BYTES_PER_LIMB;
}
