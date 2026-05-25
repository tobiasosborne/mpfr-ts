/**
 * reference_ports/broken/mpfr_modf.ts -- deliberately-buggy.
 *
 * **Collapses the decision tree to a constant output**: every input
 * returns {iop: posZero(iprec), fop: posZero(fprec)} with ternary 0
 * for both. Every non-zero case fails; signed-zero inputs that happen
 * to match by accident still fail on the sign check (since the broken
 * port always returns +0). Composite well below 0.30.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError, PREC_MAX, PREC_MIN, posZero,
} from '../../../src/core.ts';
import type { MPFR } from '../../../src/core.ts';

export function mpfr_modf(
  x: MPFR,
  iprec: bigint,
  fprec: bigint,
  rnd: RoundingMode,
): { iop: Result; fop: Result } {
  if (typeof iprec !== 'bigint' || iprec < PREC_MIN || iprec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_modf: bad iprec`);
  }
  if (typeof fprec !== 'bigint' || fprec < PREC_MIN || fprec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_modf: bad fprec`);
  }
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `mpfr_modf: unknown rnd`);
  }
  return {
    iop: { value: posZero(iprec), ternary: 0 },
    fop: { value: posZero(fprec), ternary: 0 },
  };
}
