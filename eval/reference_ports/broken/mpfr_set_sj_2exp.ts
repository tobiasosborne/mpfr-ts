/**
 * reference_ports/broken/mpfr_set_sj_2exp.ts -- deliberately-buggy.
 *
 * **Collapses the entire decision tree to a constant output** (per
 * HANDOFF gotcha #10): every input returns posZero(prec) with
 * ternary=0. Composite well below 0.30 across all 5 tag classes.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError, PREC_MAX, PREC_MIN, posZero,
} from '../../../src/core.ts';

const INT64_MIN = -(1n << 63n);
const INT64_MAX = (1n << 63n) - 1n;

export function mpfr_set_sj_2exp(
  j: bigint, e: bigint, prec: bigint, rnd: RoundingMode,
): Result {
  if (typeof j !== 'bigint' || j < INT64_MIN || j > INT64_MAX) {
    throw new MPFRError('EDOMAIN', `mpfr_set_sj_2exp: bad j`);
  }
  if (typeof e !== 'bigint' || typeof prec !== 'bigint' ||
      prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_set_sj_2exp: bad e or prec`);
  }
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `mpfr_set_sj_2exp: unknown rnd`);
  }
  // BUG: collapse to a constant output regardless of inputs.
  return { value: posZero(prec), ternary: 0 };
}
