/**
 * ops/set_z_2exp.ts -- pure-TS port of MPFR's `mpfr_set_z_2exp`.
 *
 * Set the result to `z * 2^e`, correctly rounded to `prec` bits per `rnd`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_z_2exp(mpfr_t rop, mpz_srcptr z, mpfr_exp_t e, mpfr_rnd_t rnd);
 *
 *   Body (mpfr/src/set_z_2exp.c L27-L198): sign / magnitude split on z;
 *   z=0 short-circuit emits +0; otherwise compute the MPFR-convention
 *   exponent as `zn * GMP_NUMB_BITS + e - count_leading_zeros(top_limb)`
 *   and either pad losslessly (when `prec >= bitLength(|z|)`) or round
 *   via mpn_rshift + the rounding-bit/sticky-bit dance. The C side
 *   additionally clamps against `__gmpfr_emin / __gmpfr_emax`
 *   (set_z_2exp.c L52-L55) which the TS schema does not surface; see the
 *   `@divergence` note below.
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_z_2exp(z: bigint, e: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `z`, `e`, `prec`, `rnd`.
 *   2. `z === 0n`: return `posZero(prec)`, ternary 0. Sign forced positive
 *      per mpfr/src/set_z_2exp.c L39-L41 (the bigint side has no -0n, so
 *      this matches naturally).
 *   3. Split sign / magnitude. `srcPrec = bitLength(|z|)`. The mantissa
 *      `|z|` is already MSB-aligned to exactly srcPrec bits.
 *   4. With the schema's value formula `sign * mant * 2^(exp - prec)`,
 *      the input value `sign * |z| * 2^e` corresponds to
 *      `exp = srcPrec + e` (the only delta from `mpfr_set_z`, which is
 *      the `e=0` specialisation per mpfr/src/set_z.c L24-L29).
 *   5. Lossless padding when `prec >= srcPrec`; otherwise delegate to
 *      `roundMantissa`.
 *
 * @divergence
 * No emax/emin clamp. Per ADR 0003 invariant 4: the TS schema's
 * `MPFR.exp` is an unbounded `bigint`, so we emit the value at its
 * mathematical exponent and let the golden drivers stay within the
 * default MPFR range (astronomical) so the two sides agree.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_z_2exp.c L27-L198 -- C reference body.
 *   - mpfr/src/set_z.c L24-L29 -- `mpfr_set_z` is the `e=0` delegate.
 *   - src/ops/set_z.ts -- the `e=0` specialisation; structural template.
 *   - docs/adr/0003-mpz-api.md -- API decision (`z, e` as bigint).
 *   - src/core.ts -- locked schema.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Bit length of a non-negative bigint. 64-bit chunked peel, then refine.
 * Matches `src/ops/set_z.ts`'s helper.
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

/**
 * Compute `z * 2^e` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_set_z_2exp
 *
 * @param z     the integer mantissa input, as `bigint`.
 * @param e     the binary exponent, as `bigint`.
 * @param prec  output precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on non-bigint `z`/`e` or bad precision;
 *                    `EROUND` on bad rounding mode.
 *
 * @example
 *   mpfr_set_z_2exp(0n, 5n, 53n, 'RNDN');       // posZero(53n); e ignored.
 *   mpfr_set_z_2exp(17n, 10n, 53n, 'RNDN');     // 17 * 2^10 = 17408 (lossless).
 *   mpfr_set_z_2exp(-1n, 0n, 53n, 'RNDN');      // -1.0 at prec 53.
 */
export function mpfr_set_z_2exp(
  z: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(z, e, prec, rnd);

  if (z === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  const sign: Sign = z < 0n ? -1 : 1;
  const absZ: bigint = z < 0n ? -z : z;

  const srcPrec = bitLength(absZ);
  const srcMant = absZ;
  // Schema's value formula `sign * mant * 2^(exp - prec)` with
  // mant=absZ and prec=srcPrec gives exp = srcPrec + e for input
  // value sign * absZ * 2^e.
  const srcExp = srcPrec + e;

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
