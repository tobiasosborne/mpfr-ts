/**
 * reference_ports/broken/mpfr_set_uj_2exp.ts -- deliberately-buggy.
 *
 * **BUG: srcExp computed as e (not srcPrec + e).** Drops the
 * bitLength contribution to the MPFR-convention exponent. Every
 * non-trivial case produces a wrong exp; composite well below 0.30.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError, PREC_MAX, PREC_MIN, posZero,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

const UINT64_MAX = (1n << 64n) - 1n;

function bitLength(n: bigint): bigint {
  if (n === 0n) return 0n;
  let bits = 0n;
  let probe = n;
  while (probe >= 0x10000000000000000n) { bits += 64n; probe >>= 64n; }
  while (probe > 0n) { bits++; probe >>= 1n; }
  return bits;
}

export function mpfr_set_uj_2exp(
  j: bigint, e: bigint, prec: bigint, rnd: RoundingMode,
): Result {
  if (typeof j !== 'bigint' || j < 0n || j > UINT64_MAX) {
    throw new MPFRError('EDOMAIN', `mpfr_set_uj_2exp: bad j`);
  }
  if (typeof e !== 'bigint' || typeof prec !== 'bigint' ||
      prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_set_uj_2exp: bad e or prec`);
  }
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `mpfr_set_uj_2exp: unknown rnd`);
  }
  if (j === 0n) return { value: posZero(prec), ternary: 0 };

  const srcPrec = bitLength(j);
  const srcMant = j;
  // BUG: should be srcPrec + e.
  const srcExp = e;

  if (prec >= srcPrec) {
    const padShift = prec - srcPrec;
    return {
      value: { kind: 'normal', sign: 1, prec, exp: srcExp, mant: srcMant << padShift },
      ternary: 0,
    };
  }
  const { mant, exp, ternary } = roundMantissa(srcMant, srcPrec, srcExp, prec, 1, rnd);
  return { value: { kind: 'normal', sign: 1, prec, exp, mant }, ternary };
}
