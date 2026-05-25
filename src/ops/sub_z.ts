/**
 * ops/sub_z.ts -- pure-TS port of MPFR's `mpfr_sub_z`.
 *
 * Compute `x - z` at `prec` bits per `rnd`, returning `{value, ternary}`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sub_z(mpfr_ptr y, mpfr_srcptr x, mpz_srcptr z, mpfr_rnd_t rnd);
 *
 *   Body (mpfr/src/gmp_op.c L114-L121): same shape as `mpfr_add_z` --
 *   slong fast path via `mpfr_sub_si`, otherwise lossless lift via
 *   `init_set_z` then `mpfr_sub`.
 *
 * TS signature
 * ------------
 *
 *   mpfr_sub_z(x: MPFR, z: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 * Mirrors `mpfr_add_z` structurally. The slong fast path is again
 * load-bearing for signed-zero correctness: the fallback's
 * `mpfr_set_z(0n, ...)` produces `+0`, and `mpfr_sub(-0, +0, RNDN) -> -0`
 * but `mpfr_sub(+0, +0, RNDN) -> +0` -- match the C dispatch to avoid
 * the wrong-zero edge in the latter direction (the C `mpfr_sub_si`
 * branch is what actually produces correct signed-zero handling for
 * `mpfr_sub_z(+0, 0n, RNDN) -> +0` etc.).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/gmp_op.c L114-L121 -- C reference.
 *   - src/ops/set_z.ts -- canonical bigint-to-MPFR coercion.
 *   - src/ops/sub.ts -- the underlying mpfr-mpfr sub.
 *   - src/ops/sub_si.ts -- the slong fast path.
 *   - docs/adr/0003-mpz-api.md -- API decision.
 *   - src/core.ts -- locked schema.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../core.ts';
import { mpfr_set_z } from './set_z.ts';
import { mpfr_sub } from './sub.ts';
import { mpfr_sub_si } from './sub_si.ts';

const SLONG_MIN: bigint = -(1n << 63n);
const SLONG_MAX: bigint = (1n << 63n) - 1n;

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
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof z !== 'bigint') {
    throw new MPFRError('EPREC', `z must be bigint, got ${typeof z}`);
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
 * Compute `x - z` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_sub_z
 *
 * @param x     the MPFR operand.
 * @param z     the integer operand, as `bigint` of any magnitude.
 * @param prec  output precision in bits.
 * @param rnd   one of the five rounding modes.
 *
 * @returns     `{value, ternary}`.
 *
 * @throws {MPFRError} `EPREC` on bad `z` / `prec`; `EROUND` on bad rnd.
 */
export function mpfr_sub_z(
  x: MPFR,
  z: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(z, prec, rnd);

  if (z >= SLONG_MIN && z <= SLONG_MAX) {
    return mpfr_sub_si(x, z, prec, rnd);
  }
  const absZ = z < 0n ? -z : z;
  const intPrec = bitLength(absZ);
  const tPrec = intPrec < PREC_MIN ? PREC_MIN : intPrec;
  const t = mpfr_set_z(z, tPrec, 'RNDN').value;
  return mpfr_sub(x, t, prec, rnd);
}
