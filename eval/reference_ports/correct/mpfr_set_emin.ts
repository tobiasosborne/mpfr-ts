/**
 * reference_ports/correct/mpfr_set_emin.ts -- mutation-prove reference
 * for mpfr_set_emin (mirror of mpfr_set_emax).
 *
 * Per CLAUDE.md PIL.3 the calibration baseline. The production
 * src/ops/set_emin.ts does not yet exist.
 *
 * Algorithm (mpfr/src/exceptions.c L38-L50):
 *   if (exp >= MPFR_EMIN_MIN && exp <= MPFR_EMIN_MAX) {
 *       __gmpfr_emin = exp; return 0;
 *   } else { return 1; }
 *
 * Immutable-API lift: return {ret, emin} where emin is the resulting
 * global exponent floor. On accept emin == exp; on reject emin is the
 * unchanged prior emin, pinned by the golden to the MPFR default
 * (MPFR_EMIN_DEFAULT = -(2^30 - 1)).
 *
 * Host bounds (mpfr/src/mpfr-impl.h L1037,L1048-L1049; mpfr_exp_t == long):
 *   MPFR_EMIN_MIN = -(2^62 - 1), MPFR_EMIN_MAX = 2^62 - 1.
 *
 * Ref: mpfr/src/exceptions.c L38-L50 -- C reference.
 * Ref: /usr/include/mpfr.h L216-L217 -- MPFR_EMIN_DEFAULT prior baseline.
 * Ref: eval/functions/mpfr_set_emin/spec.json -- contract.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_EMIN_MIN = -((2n ** 62n) - 1n);
const MPFR_EMIN_MAX = (2n ** 62n) - 1n;
/** Prior emin the golden runs each case from (MPFR_EMIN_DEFAULT). */
const MPFR_EMIN_DEFAULT = -((1n << 30n) - 1n);

export function mpfr_set_emin(exp: bigint): { ret: number; emin: bigint } {
  if (typeof exp !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_emin: exp must be bigint, got ${typeof exp}`,
    );
  }
  // Mirror the C bounds check exactly (inclusive both ends).
  if (exp >= MPFR_EMIN_MIN && exp <= MPFR_EMIN_MAX) {
    return { ret: 0, emin: exp };
  }
  // Reject: global unchanged -> report the prior (default) emin.
  return { ret: 1, emin: MPFR_EMIN_DEFAULT };
}
