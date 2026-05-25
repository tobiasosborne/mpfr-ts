/**
 * reference_ports/broken/mpfr_custom_get_exp.ts -- deliberately-buggy.
 *
 * **BUG: returns `x.exp - x.prec` instead of `x.exp`.** Plausible
 * confusion: the schema documents that `value = sign * mant * 2^(exp -
 * prec)` (see src/core.ts L62-L63), so an agent might mistakenly think
 * the "real" exponent is `exp - prec` rather than `exp`. It is not --
 * the schema's `exp` IS the unbiased base-2 exponent (mpfr_get_exp's
 * convention). This bug shifts every output by -prec, making nearly
 * every case fail.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_custom_get_exp(x: MPFR): bigint {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', `mpfr_custom_get_exp: x must be MPFR`);
  }
  if (x.kind !== 'normal') {
    throw new MPFRError('EDOMAIN', `mpfr_custom_get_exp: singular`);
  }
  // BUG: should be `return x.exp`. Returning exp-prec mistakes the
  // mantissa-relative base for the unbiased exponent.
  return x.exp - x.prec;
}
