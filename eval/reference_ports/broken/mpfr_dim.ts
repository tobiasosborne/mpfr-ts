/**
 * reference_ports/broken/mpfr_dim.ts — deliberately-buggy mpfr_dim.
 *
 * **Multi-bug perturbation:**
 *   1. For b > c: delegates to mpfr_add(b, c, prec, rnd) instead of
 *      mpfr_sub. The arithmetic is fundamentally wrong.
 *   2. For b == c: returns negZero(prec) instead of posZero. Sign flip
 *      on the zero branch.
 *   3. For NaN inputs: returns posZero(prec) instead of NAN_VALUE.
 *      NaN-propagation broken.
 *
 * NOT used in production.
 *
 * Ref: src/ops/dim.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  negZero,
  posZero,
} from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';
import { mpfr_add } from '../../../src/ops/add.ts';

function validateArgs(prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') throw new MPFRError('EPREC', `prec must be bigint`);
  if (prec < PREC_MIN || prec > PREC_MAX) throw new MPFRError('EPREC', `prec out of range`);
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

export function mpfr_dim(
  b: MPFR,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  if (b.kind === 'nan' || c.kind === 'nan') {
    // BUG 3: should be NAN_VALUE.
    return { value: posZero(prec), ternary: 0 };
  }

  const cmp = compareMPFR(b, c);
  if (cmp === null) throw new MPFRError('EDOMAIN', 'unexpected');
  if (cmp > 0) {
    // BUG 1: should be mpfr_sub.
    return mpfr_add(b, c, prec, rnd);
  }
  // BUG 2: should be posZero.
  return { value: negZero(prec), ternary: 0 };
}
