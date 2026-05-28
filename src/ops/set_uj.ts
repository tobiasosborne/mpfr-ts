/**
 * mpfr_set_uj -- port of MPFR's mpfr_set_uj.
 *
 * Set the result to the unsigned integer j, correctly rounded to prec bits
 * per rnd, where j is a C uintmax_t (uint64 on the platforms we care about).
 *
 * Algorithm (mpfr/src/set_uj.c L31-L35):
 *   return mpfr_set_uj_2exp(x, j, 0, rnd);
 *
 * The TS port mirrors this: it delegates to the already-shipped
 * mpfr_set_uj_2exp with e = 0n.
 *
 * Ref: mpfr/src/set_uj.c L31-L35 -- C reference body (one-line delegate).
 * Ref: src/ops/set_uj_2exp.ts -- the underlying delegate.
 */

import type { Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from '../core.ts';
import { mpfr_set_uj_2exp } from './set_uj_2exp.ts';

const UINT64_MAX = (1n << 64n) - 1n;

function validateArgs(j: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof j !== 'bigint') {
    throw new MPFRError('EPREC', `j must be bigint, got ${typeof j}`);
  }
  if (j < 0n) {
    throw new MPFRError('EDOMAIN', `j must be >= 0 (uintmax_t domain), got ${j}`);
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
