/**
 * reference_ports/correct/mpfr_set_uj.ts -- mutation-prove reference.
 *
 * Set the result to the unsigned integer j, correctly rounded to prec
 * bits per rnd. j is an unsigned uint64 (uintmax_t analog) constrained
 * to [0, 2^64 - 1].
 *
 * Algorithm (mpfr/src/set_uj.c L31-L35):
 *   int mpfr_set_uj (mpfr_ptr x, uintmax_t j, mpfr_rnd_t rnd) {
 *     return mpfr_set_uj_2exp (x, j, 0, rnd);
 *   }
 *
 * The TS port mirrors this exactly: delegate to the already-shipped
 * src/ops/set_uj_2exp.ts with e = 0n. Unlike set_sj there is no
 * sign-flip / rounding-invert dance -- the input is non-negative, so
 * the underlying set_uj_2exp handles j=0 (-> +0) and positive normals
 * directly.
 *
 * Ref: mpfr/src/set_uj.c L31-L35 -- C reference body (one-line delegate).
 * Ref: src/ops/set_uj_2exp.ts -- the underlying delegate.
 * Ref: eval/reference_ports/correct/mpfr_set_sj.ts -- signed sibling.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../../../src/core.ts';
import { mpfr_set_uj_2exp } from '../../../src/ops/set_uj_2exp.ts';

const UINT64_MAX = (1n << 64n) - 1n;

function validateArgs(j: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof j !== 'bigint') {
    throw new MPFRError('EPREC', `j must be bigint, got ${typeof j}`);
  }
  if (j < 0n) {
    throw new MPFRError('EDOMAIN', `j must be non-negative (uintmax_t), got ${j}`);
  }
  if (j > UINT64_MAX) {
    throw new MPFRError('EDOMAIN', `j must be <= 2^64 - 1 (uintmax_t domain), got ${j}`);
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

export function mpfr_set_uj(
  j: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(j, prec, rnd);
  return mpfr_set_uj_2exp(j, 0n, prec, rnd);
}
