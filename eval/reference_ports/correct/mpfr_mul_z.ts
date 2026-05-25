/**
 * reference_ports/correct/mpfr_mul_z.ts -- hand-written reference port.
 *
 * Compute x * z at prec bits per rnd, returning {value, ternary}.
 *
 * Structurally identical to mpfr_add_z / mpfr_sub_z: lift z to a
 * lossless intermediate MPFR via mpfr_set_z at prec =
 * max(bitLength(|z|), 1n), then delegate to mpfr_mul.
 *
 * Ref: mpfr/src/gmp_op.c L87-L94 -- C reference.
 * Ref: src/ops/set_z.ts -- canonical bigint-to-MPFR coercion.
 * Ref: src/ops/mul.ts -- the underlying mpfr*mpfr mul.
 * Ref: docs/adr/0003-mpz-api.md -- the API decision.
 * Ref: CLAUDE.md PIL.3 -- mutation-prove against this file.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../../../src/core.ts';
import { mpfr_set_z } from '../../../src/ops/set_z.ts';
import { mpfr_mul } from '../../../src/ops/mul.ts';
import { mpfr_mul_si } from '../../../src/ops/mul_si.ts';

// signed long range — mirrors mpfr/src/gmp_op.c L88-L94 dispatch.
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
  x: MPFR,
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
  void x;
}

export function mpfr_mul_z(
  x: MPFR,
  z: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(x, z, prec, rnd);

  // Mirror C dispatch (mpfr/src/gmp_op.c L88-L94).
  if (z >= SLONG_MIN && z <= SLONG_MAX) {
    return mpfr_mul_si(x, z, prec, rnd);
  }
  const absZ = z < 0n ? -z : z;
  const intPrec = bitLength(absZ);
  const tPrec = intPrec < PREC_MIN ? PREC_MIN : intPrec;
  const t = mpfr_set_z(z, tPrec, 'RNDN').value;
  return mpfr_mul(x, t, prec, rnd);
}
