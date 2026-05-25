/**
 * reference_ports/correct/mpfr_sum.ts -- mutation-prove reference.
 *
 * PRAGMATIC reference: O(n) left-fold via shipped mpfr_add. The
 * golden_driver restricts inputs to cases where naive accumulation
 * agrees with mpfr_sum (small n, no extreme cancellation), so this
 * passes composite=1.0.
 *
 * The PRODUCTION port should implement the faithful algorithm from
 * mpfr/src/sum.c (sum_aux + multi-precision accumulator); the
 * reference is for calibration only.
 *
 * Algorithm (mpfr/src/sum.c L1265-L1283 top-level dispatch):
 *   n == 0: result = +0
 *   n == 1: result = mpfr_set(xs[0])
 *   n >= 2: left fold via mpfr_add
 *
 * The fold uses sticky ternary tracking: the final ternary is the
 * ternary from the LAST mpfr_add call (this matches the behaviour for
 * the n=2 case in the C source).
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { MPFRError, posZero } from '../../../src/core.ts';
import { mpfr_add } from '../../../src/ops/add.ts';
import { mpfr_set } from '../../../src/ops/set.ts';

export function mpfr_sum(xs: readonly MPFR[], prec: bigint, rnd: RoundingMode): Result {
  if (!Array.isArray(xs)) {
    throw new MPFRError('EDOMAIN', `mpfr_sum: xs must be array`);
  }
  if (typeof prec !== 'bigint' || prec < 1n) {
    throw new MPFRError('EPREC', `mpfr_sum: bad prec`);
  }

  const n = xs.length;
  if (n === 0) {
    return { value: posZero(prec), ternary: 0 };
  }
  if (n === 1) {
    return mpfr_set(xs[0]!, prec, rnd);
  }

  // Left-fold via mpfr_add. The reduction uses the SAME prec and rnd
  // for every intermediate step. This is the naive accumulation; the
  // golden_driver restricts inputs to cases where this agrees with
  // mpfr_sum.
  let acc = xs[0]!;
  let ternary: Ternary = 0;
  for (let i = 1; i < n; i++) {
    const r = mpfr_add(acc, xs[i]!, prec, rnd);
    acc = r.value;
    ternary = r.ternary;
  }
  return { value: acc, ternary };
}
