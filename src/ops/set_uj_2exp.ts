/**
 * ops/set_uj_2exp.ts -- pure-TS port of MPFR's `mpfr_set_uj_2exp`.
 *
 * Set the result to `j * 2^e`, correctly rounded to `prec` bits per `rnd`,
 * where `j` is an unsigned integer (`uintmax_t` in the C signature).
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_uj_2exp(mpfr_t rop, uintmax_t j, intmax_t e, mpfr_rnd_t rnd);
 *
 *   Body (mpfr/src/set_uj.c L36-L132): j=0 short-circuit emits +0;
 *   otherwise decompose `j` into MPFR's mantissa convention by computing
 *   the bit length and applying either a lossless pad or the rounding
 *   dance. The C side additionally clamps against `__gmpfr_emin /
 *   __gmpfr_emax` which the TS schema does not surface; see the
 *   `@divergence` note below.
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_uj_2exp(j: bigint, e: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `j` (bigint in [0n, 2^64-1n]), `e`, `prec`, `rnd`.
 *   2. `j === 0n`: return `posZero(prec)`, ternary 0. `e` is ignored
 *      (matches mpfr/src/set_uj.c L48-L53).
 *   3. `srcPrec = bitLength(j)`. Per the schema's value formula
 *      `sign * mant * 2^(exp - prec)`, the input `+j * 2^e` corresponds
 *      to `exp = srcPrec + e` with `mant = j` at `prec = srcPrec`.
 *   4. Lossless padding when `prec >= srcPrec`; otherwise delegate to
 *      `roundMantissa` with `sign = 1`.
 *
 * Sign is always positive: `j >= 0n` by validation, and the `j = 0n`
 * short-circuit handles the zero case explicitly (MPFR emits `+0`, never
 * `-0`, from a non-negative input).
 *
 * @divergence
 *
 * 1. `j` domain. The C `uintmax_t` is platform-dependent; on x86_64
 *    Linux it is `unsigned long long == 2^64 - 1`. The TS port pins the
 *    domain to `[0n, 2^64 - 1n]` so goldens are reproducible across
 *    platforms and so the validation matches the upper-bound MPFR
 *    typically sees in practice. A bigger `j` is rejected with
 *    `EDOMAIN`; if a future use case needs `uintmax_t > 64`, it should
 *    go through an ADR.
 *
 * 2. No emax/emin clamp. Per ADR 0003 invariant 4: the TS schema's
 *    `MPFR.exp` is an unbounded `bigint`, so we emit the value at its
 *    mathematical exponent and let the golden drivers stay within the
 *    default MPFR range (astronomical) so the two sides agree.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_uj.c L36-L132 -- C reference body.
 *   - src/ops/set_z_2exp.ts -- signed sibling (`mpz_t`-input variant);
 *     same lossless-pad-vs-roundMantissa fork.
 *   - eval/reference_ports/correct/mpfr_set_uj_2exp.ts -- mutation-prove ref.
 *   - eval/functions/mpfr_set_uj_2exp/spec.json -- contract.
 *   - src/core.ts -- locked schema.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/** Upper bound of `uintmax_t` on x86_64 Linux: 2^64 - 1. */
const UINT64_MAX = (1n << 64n) - 1n;

/**
 * Bit length of a non-negative bigint. 64-bit chunked peel, then refine.
 * Matches `src/ops/set_z_2exp.ts`'s helper.
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
  j: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof j !== 'bigint') {
    throw new MPFRError('EPREC', `j must be bigint, got ${typeof j}`);
  }
  if (j < 0n) {
    throw new MPFRError(
      'EDOMAIN',
      `j must be non-negative (uintmax_t), got ${j}`,
    );
  }
  if (j > UINT64_MAX) {
    throw new MPFRError(
      'EDOMAIN',
      `j must be <= 2^64-1 (uint64 domain), got ${j}`,
    );
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
 * Compute `j * 2^e` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_set_uj_2exp
 *
 * @param j     the unsigned integer mantissa input, as `bigint`. Must
 *              lie in `[0n, 2^64 - 1n]` (the uint64 domain); see the
 *              `@divergence` note in the module docstring.
 * @param e     the binary exponent, as `bigint`.
 * @param prec  output precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}` per src/core.ts.
 *
 * @throws {MPFRError} `EPREC` on non-bigint `j`/`e`/`prec` or bad
 *                    precision; `EDOMAIN` on out-of-range `j`;
 *                    `EROUND` on bad rounding mode.
 *
 * @example
 *   mpfr_set_uj_2exp(0n, 5n, 53n, 'RNDN');     // posZero(53n); e ignored.
 *   mpfr_set_uj_2exp(17n, 10n, 53n, 'RNDN');   // 17 * 2^10 = 17408.
 *   mpfr_set_uj_2exp(1n, 0n, 53n, 'RNDN');     // +1.0 at prec 53.
 */
export function mpfr_set_uj_2exp(
  j: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(j, e, prec, rnd);

  if (j === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  const srcPrec = bitLength(j);
  const srcMant = j;
  // Schema's value formula `sign * mant * 2^(exp - prec)` with sign=+1,
  // mant=j, prec=srcPrec gives exp = srcPrec + e for input value
  // (+1) * j * 2^e.
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
    1, // always positive
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
