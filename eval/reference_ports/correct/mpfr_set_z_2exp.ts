/**
 * reference_ports/correct/mpfr_set_z_2exp.ts -- hand-written reference port.
 *
 * Sets the result to z * 2^e, correctly rounded to prec bits per rnd.
 * Algorithmically identical to src/ops/set_z.ts with one delta: the
 * MPFR-convention exponent of the value is srcPrec + e rather than
 * just srcPrec (set_z is the e=0 specialisation, per mpfr/src/set_z.c
 * L24-L29). The lossless-pad and roundMantissa paths are otherwise
 * unchanged.
 *
 * The schema's value formula (src/core.ts L51-L62) is
 *
 *     sign * mant * 2^(exp - prec)
 *
 * For an input (z, e) we have value = sign * absZ * 2^e. With
 * mant = absZ at prec = srcPrec = bitLength(|z|), this gives
 * exp = srcPrec + e in MPFR convention.
 *
 * Ref: mpfr/src/set_z_2exp.c L27-L198 -- C reference.
 * Ref: src/ops/set_z.ts -- e=0 specialisation; structural template.
 * Ref: docs/adr/0003-mpz-api.md -- API decision (z, e as bigint).
 * Ref: CLAUDE.md PIL.3 -- mutation-prove against this file.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

/**
 * Bit length of a non-negative bigint. Chunked 64 bits at a time,
 * then refined within the top limb. Same shape as set_z.ts.
 */
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

function validateArgs(
  z: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof z !== 'bigint') {
    throw new MPFRError('EPREC', `z must be bigint, got ${typeof z}`);
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
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

export function mpfr_set_z_2exp(
  z: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(z, e, prec, rnd);

  // z=0 short-circuit: sign forced positive; e ignored.
  // Ref: mpfr/src/set_z_2exp.c L39-L41.
  if (z === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  const sign: Sign = z < 0n ? -1 : 1;
  const absZ: bigint = z < 0n ? -z : z;

  const srcPrec = bitLength(absZ);
  const srcMant = absZ;
  // The value is sign * absZ * 2^e. Schema's formula gives
  //   exp = srcPrec + e
  // for mant = absZ at prec = srcPrec.
  const srcExp = srcPrec + e;

  // Lossless padding when prec >= srcPrec.
  if (prec >= srcPrec) {
    const padShift = prec - srcPrec;
    const value: MPFR = {
      kind: 'normal',
      sign,
      prec,
      exp: srcExp,
      mant: srcMant << padShift,
    };
    return { value, ternary: 0 };
  }

  // Lossy rounding when prec < srcPrec.
  const { mant, exp, ternary } = roundMantissa(
    srcMant,
    srcPrec,
    srcExp,
    prec,
    sign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
