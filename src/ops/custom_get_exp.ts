/**
 * ops/custom_get_exp.ts -- pure-TS port of MPFR's `mpfr_custom_get_exp`.
 *
 * Returns the raw exponent field of a normal MPFR value. The C body is
 * one line: `return MPFR_EXP(x)`. For normals this is the same value
 * `mpfr_get_exp` returns; for singulars C returns sentinel exponents
 * (`__MPFR_EXP_NAN`, `__MPFR_EXP_INF`, `__MPFR_EXP_ZERO`) that have no
 * analogue in the locked schema -- singular inputs are therefore
 * out-of-scope and the port fails fast.
 *
 * Ref: mpfr/src/stack_interface.c L46-L51 -- C reference body.
 * Ref: mpfr/src/mpfr.h L243-L245 -- sentinel exponent constants.
 * Ref: src/core.ts L113-L135 -- MPFR.exp; 0n by convention for singulars.
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Read the base-2 exponent of a normal MPFR value.
 *
 * @mpfrName mpfr_custom_get_exp
 *
 * @param x  A normal MPFR. Singular inputs (NaN/Inf/Zero) throw EDOMAIN
 *           because the locked schema carries no sentinel exponents that
 *           could disambiguate them from legitimate values.
 * @returns  The unbiased base-2 exponent.
 */
export function mpfr_custom_get_exp(x: MPFR): bigint {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'mpfr_custom_get_exp: x must be an MPFR object');
  }
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_custom_get_exp: singular inputs out of scope (kind=${x.kind})`,
    );
  }
  return x.exp;
}
