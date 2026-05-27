/**
 * mpfr_set_sj -- port of MPFR's mpfr_set_sj.
 *
 * Set the result to j, correctly rounded to prec bits per rnd, where j
 * is a signed C intmax_t (int64 on the platforms we care about).
 *
 * Algorithm (mpfr/src/set_sj.c L27-L31):
 *   return mpfr_set_sj_2exp(x, j, 0, rnd);
 *
 * The TS port mirrors this: it delegates to the already-shipped
 * mpfr_set_sj_2exp with e = 0n.
 *
 * Ref: mpfr/src/set_sj.c L27-L31 -- C reference body (one-line delegate).
 * Ref: src/ops/set_sj_2exp.ts -- the underlying delegate.
 */

import type { Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from '../core.ts';
import { mpfr_set_sj_2exp } from './set_sj_2exp.ts';

const INT64_MIN = -(1n << 63n);
const INT64_MAX = (1n << 63n) - 1n;

function validateArgs(j: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof j !== 'bigint') {
    throw new MPFRError('EPREC', `j must be bigint, got ${typeof j}`);
  }
  if (j < INT64_MIN) {
    throw new MPFRError('EDOMAIN', `j must be >= -(2^63) (intmax_t domain), got ${j}`);
  }
  if (j > INT64_MAX) {
    throw new MPFRError('EDOMAIN', `j must be <= 2^63 - 1 (intmax_t domain), got ${j}`);
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
