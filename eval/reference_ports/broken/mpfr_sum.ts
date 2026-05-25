/**
 * reference_ports/broken/mpfr_sum.ts -- deliberately-buggy.
 *
 * **BUG: returns +0 for every input.** Strongest perturbation: ignore
 * the input array entirely. Strengthened from earlier "only n<=1 broken"
 * variant (which left most n>=2 cases passing because the delegating
 * left-fold matched libmpfr on the restricted golden domain).
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError, posZero } from '../../../src/core.ts';

export function mpfr_sum(xs: readonly MPFR[], prec: bigint, rnd: RoundingMode): Result {
  if (!Array.isArray(xs)) {
    throw new MPFRError('EDOMAIN', `mpfr_sum: xs must be array`);
  }
  if (typeof prec !== 'bigint' || prec < 1n) {
    throw new MPFRError('EPREC', `mpfr_sum: bad prec`);
  }
  // rnd validation kept so we don't throw on legitimate inputs in a way
  // that obscures the value-mismatch signal.
  void rnd;
  // BUG: ignores xs entirely; returns +0 always.
  return { value: posZero(prec), ternary: 0 };
}
