/**
 * reference_ports/broken/mpfr_cbrt.ts -- deliberately-buggy.
 *
 * **BUG: returns x unchanged (no cube root).** Collapse-to-identity.
 * Every non-trivial case (most of the golden) yields a wrong value;
 * the cube root of 8 is not 8. Cases that pass: cbrt(0) = 0, cbrt(1) = 1
 * (where x equals its own cube root). NaN/Inf cases also pass trivially.
 *
 * Composite expected well below 0.30.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError, PREC_MIN, PREC_MAX, NAN_VALUE } from '../../../src/core.ts';

export function mpfr_cbrt(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  if (typeof prec !== 'bigint' || prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_cbrt: bad prec`);
  }
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `mpfr_cbrt: unknown rnd`);
  }
  if (x.kind === 'nan') return { value: NAN_VALUE, ternary: 0 };
  // BUG: return x retargeted to the new prec (effectively cbrt = identity).
  // For singular cases this works; for normal it's the input mantissa unchanged.
  return {
    value: { ...x, prec },
    ternary: 0,
  };
}
