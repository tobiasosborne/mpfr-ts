/**
 * reference_ports/correct/mpfr_set_uj_2exp.ts -- mutation-prove reference.
 *
 * Sets the result to j * 2^e, correctly rounded. j is an unsigned
 * bigint (uintmax_t analog) constrained to [0n, 2^64 - 1n].
 *
 * Algorithmically the unsigned variant of mpfr_set_z_2exp: same
 * lossless-pad-vs-roundMantissa fork, sign is always +1 for j > 0n,
 * +0 for j == 0n.
 *
 * The schema's value formula: sign * mant * 2^(exp - prec). For input
 * (j, e): value = +1 * j * 2^e. With mant = j at prec = srcPrec =
 * bitLength(j), srcExp = srcPrec + e in MPFR convention.
 *
 * Ref: mpfr/src/set_uj.c L36-L132 -- C reference.
 * Ref: eval/reference_ports/correct/mpfr_set_z_2exp.ts -- signed sibling
 *      (structural template).
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

const UINT64_MAX = (1n << 64n) - 1n;

function bitLength(n: bigint): bigint {
  if (n === 0n) return 0n;
  let bits = 0n;
  let probe = n;
  while (probe >= 0x10000000000000000n /* 2^64 */) {
    bits += 64n;
    probe >>= 64n;
  }
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

function validateArgs(j: bigint, e: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof j !== 'bigint') {
    throw new MPFRError('EPREC', `j must be bigint, got ${typeof j}`);
  }
  if (j < 0n) {
    throw new MPFRError('EDOMAIN', `j must be non-negative (uintmax_t), got ${j}`);
  }
  if (j > UINT64_MAX) {
    throw new MPFRError('EDOMAIN', `j must be <= 2^64-1 (uint64 domain), got ${j}`);
  }
  if (typeof e !== 'bigint') {
    throw new MPFRError('EPREC', `e must be bigint, got ${typeof e}`);
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

export function mpfr_set_uj_2exp(
  j: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(j, e, prec, rnd);

  // j=0 short-circuit: result is +0, e ignored (per set_uj.c L48-L53).
  if (j === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  const srcPrec = bitLength(j);
  const srcMant = j;
  const srcExp = srcPrec + e;

  if (prec >= srcPrec) {
    const padShift = prec - srcPrec;
    const value: MPFR = {
      kind: 'normal',
      sign: 1,
      prec,
      exp: srcExp,
      mant: srcMant << padShift,
    };
    return { value, ternary: 0 };
  }

  const { mant, exp, ternary } = roundMantissa(
    srcMant,
    srcPrec,
    srcExp,
    prec,
    1,  /* always positive */
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign: 1,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
