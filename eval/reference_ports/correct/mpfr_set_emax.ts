/**
 * reference_ports/correct/mpfr_set_emax.ts -- mutation-prove reference
 * for mpfr_set_emax.
 *
 * Per CLAUDE.md PIL.3 the calibration baseline. The production
 * src/ops/set_emax.ts does not yet exist; the orchestrator materialises
 * it during the port-and-grade flow.
 *
 * Algorithm (mpfr/src/exceptions.c L74-L86):
 *   if (exp >= MPFR_EMAX_MIN && exp <= MPFR_EMAX_MAX) {
 *       __gmpfr_emax = exp; return 0;
 *   } else { return 1; }
 *
 * Immutable-API lift: instead of mutating a global and returning only the
 * 0/1 status, we return {ret, emax} where emax is the resulting global
 * exponent ceiling. On accept emax == exp; on reject emax is the
 * unchanged prior emax, which the golden pins to the MPFR default
 * (MPFR_EMAX_DEFAULT = 2^30 - 1).
 *
 * Host bounds (mpfr/src/mpfr-impl.h L1037,L1050-L1051; mpfr_exp_t == long):
 *   MPFR_EMAX_MIN = -(2^62 - 1), MPFR_EMAX_MAX = 2^62 - 1.
 *
 * Ref: mpfr/src/exceptions.c L74-L86 -- C reference.
 * Ref: /usr/include/mpfr.h L216-L217 -- MPFR_EMAX_DEFAULT prior baseline.
 * Ref: eval/functions/mpfr_set_emax/spec.json -- contract.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_EMAX_MIN = -((2n ** 62n) - 1n);
const MPFR_EMAX_MAX = (2n ** 62n) - 1n;
/** Prior emax the golden runs each case from (MPFR_EMAX_DEFAULT). */
const MPFR_EMAX_DEFAULT = (1n << 30n) - 1n;

export function mpfr_set_emax(exp: bigint): { ret: number; emax: bigint } {
  if (typeof exp !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_emax: exp must be bigint, got ${typeof exp}`,
    );
  }
  // Mirror the C bounds check exactly (inclusive both ends).
  if (exp >= MPFR_EMAX_MIN && exp <= MPFR_EMAX_MAX) {
    return { ret: 0, emax: exp };
  }
  // Reject: global unchanged -> report the prior (default) emax.
  return { ret: 1, emax: MPFR_EMAX_DEFAULT };
}
