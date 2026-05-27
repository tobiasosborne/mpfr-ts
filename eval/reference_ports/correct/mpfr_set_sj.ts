/**
 * reference_ports/correct/mpfr_set_sj.ts -- mutation-prove reference.
 *
 * Set the result to j (a signed int64), correctly rounded.
 *
 * Algorithm (mpfr/src/set_sj.c L27-L31):
 *   mpfr_set_sj(x, j, rnd) = mpfr_set_sj_2exp(x, j, 0, rnd);
 *
 * This is a one-line delegate. The TS port mirrors it exactly by
 * calling the already-shipped mpfr_set_sj_2exp with e = 0n.
 *
 * Ref: mpfr/src/set_sj.c L27-L31 -- C reference.
 * Ref: src/ops/set_sj_2exp.ts -- the underlying delegate.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from '../../../src/core.ts';
import { mpfr_set_sj_2exp } from '../../../src/ops/set_sj_2exp.ts';

const INT64_MIN = -(1n << 63n);
const INT64_MAX = (1n << 63n) - 1n;

function validateArgs(j: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof j !== 'bigint') {
    throw new MPFRError('EPREC', `j must be bigint, got ${typeof j}`);
  }
  if (j < INT64_MIN) {
    throw new MPFRError('EDOMAIN', `j must be >= -(2^63), got ${j}`);
  }
  if (j > INT64_MAX) {
    throw new MPFRError('EDOMAIN', `j must be <= 2^63 - 1, got ${j}`);
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

export function mpfr_set_sj(
  j: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(j, prec, rnd);
  return mpfr_set_sj_2exp(j, 0n, prec, rnd);
}
