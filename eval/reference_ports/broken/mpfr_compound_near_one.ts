/**
 * reference_ports/broken/mpfr_compound_near_one.ts -- deliberately-buggy.
 *
 * **BUG: always returns one with ternary=0.** Strongest perturbation:
 * collapses the entire branch tree (rounding mode and sign s direction)
 * to a single constant. Most cases have non-zero ternary, so the ternary
 * mismatch alone fails most cases; the step-away cases additionally fail
 * on value. Strengthened from earlier swap-nextabove-nextbelow variant
 * (which left fast-path cases passing).
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const VALID_RNDS: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];

function buildOne(prec: bigint): MPFR {
  return { kind: 'normal', sign: 1, prec, exp: 1n, mant: 1n << (prec - 1n) };
}

export function mpfr_compound_near_one(prec: bigint, s: number, rnd: RoundingMode): Result {
  if (typeof prec !== 'bigint' || prec < 1n) {
    throw new MPFRError('EPREC', 'mpfr_compound_near_one: bad prec');
  }
  if (s !== +1 && s !== -1) {
    throw new MPFRError('EDOMAIN', 'mpfr_compound_near_one: bad s');
  }
  if (!VALID_RNDS.includes(rnd)) {
    throw new MPFRError('EROUND', 'mpfr_compound_near_one: bad rnd');
  }
  // BUG: returns one with ternary=0 unconditionally.
  return { value: buildOne(prec), ternary: 0 };
}
