/**
 * ops/add_z.ts -- pure-TS port of MPFR's `mpfr_add_z`.
 *
 * Compute `x + z` at `prec` bits per `rnd`, returning `{value, ternary}`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_add_z(mpfr_ptr y, mpfr_srcptr x, mpz_srcptr z, mpfr_rnd_t rnd);
 *
 *   Body (mpfr/src/gmp_op.c L106-L112): when `mpz_fits_slong_p(z)` the C
 *   dispatches to `mpfr_add_si`; otherwise builds a temporary mpfr_t
 *   from z via `init_set_z` (gmp_op.c L25-L49) at precision sufficient
 *   to represent z exactly, then calls `mpfr_add`. The rounding of the
 *   result is the rounding of `mpfr_add` only -- no double-rounding.
 *
 * TS signature
 * ------------
 *
 *   mpfr_add_z(x: MPFR, z: bigint, prec: bigint, rnd: RoundingMode): Result;
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `z`, `prec`, `rnd` (x is structurally validated downstream).
 *   2. If `z` fits in a signed long, delegate to `mpfr_add_si`. This is
 *      load-bearing for signed-zero correctness: the fallback path's
 *      lossless lift via `mpfr_set_z(0n, ...)` produces `+0`, and
 *      `mpfr_add(-0, +0, RNDN) -> +0` (IEEE 754 sum), which would
 *      silently flip `mpfr_add_z(-0, 0n, RNDN)` from the C-correct `-0`.
 *   3. Otherwise lift z losslessly via `mpfr_set_z(z, max(bitLength(|z|), 1n),
 *      'RNDN').value` (per ADR 0003 invariant 3) and delegate to `mpfr_add`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/gmp_op.c L106-L112 -- C reference (`mpfr_add_z`).
 *   - mpfr/src/gmp_op.c L25-L49 -- `init_set_z` lossless lift.
 *   - src/ops/set_z.ts -- canonical bigint-to-MPFR coercion.
 *   - src/ops/add.ts -- the underlying mpfr+mpfr add.
 *   - src/ops/add_si.ts -- the slong fast path.
 *   - docs/adr/0003-mpz-api.md -- API decision.
 *   - src/core.ts -- locked schema.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../core.ts';
import { mpfr_set_z } from './set_z.ts';
import { mpfr_add } from './add.ts';
import { mpfr_add_si } from './add_si.ts';

// Signed long range -- mirrors mpfr/src/gmp_op.c L108 dispatch.
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
 * Compute `x + z` at precision `prec` per `rnd`.
 *
 * @mpfrName mpfr_add_z
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
export function mpfr_add_z(
  x: MPFR,
  z: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(z, prec, rnd);

  // Slong fast path -- critical for signed-zero correctness, see header.
  if (z >= SLONG_MIN && z <= SLONG_MAX) {
    return mpfr_add_si(x, z, prec, rnd);
  }
  const absZ = z < 0n ? -z : z;
  const intPrec = bitLength(absZ);
  const tPrec = intPrec < PREC_MIN ? PREC_MIN : intPrec;
  const t = mpfr_set_z(z, tPrec, 'RNDN').value;
  return mpfr_add(x, t, prec, rnd);
}
