/**
 * reference_ports/correct/mpfr_add_z.ts -- hand-written reference port.
 *
 * Compute x + z at prec bits per rnd, returning {value, ternary}.
 *
 * Per ADR 0003 invariant 3: lift z to a lossless intermediate MPFR
 * via mpfr_set_z at prec = max(bitLength(|z|), 1n) (the precision
 * choice guarantees ternary=0 on the conversion, matching the C
 * source's init_set_z helper -- mpfr/src/gmp_op.c L25-L49), then
 * delegate to mpfr_add. The rounding of the result is the rounding
 * of mpfr_add; no double-rounding.
 *
 * Ref: mpfr/src/gmp_op.c L106-L112 -- C reference (foo() helper).
 * Ref: src/ops/set_z.ts -- canonical bigint-to-MPFR coercion.
 * Ref: src/ops/add.ts -- the underlying mpfr+mpfr add.
 * Ref: docs/adr/0003-mpz-api.md -- the API decision.
 * Ref: CLAUDE.md PIL.3 -- mutation-prove against this file.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../../../src/core.ts';
import { mpfr_set_z } from '../../../src/ops/set_z.ts';
import { mpfr_add } from '../../../src/ops/add.ts';
import { mpfr_add_si } from '../../../src/ops/add_si.ts';

// signed long range — mirrors mpfr/src/gmp_op.c L108: when z fits in
// slong, dispatch to mpfr_add_si. This is the only correct path for
// z=0 (the lossless mpfr_set_z + mpfr_add path loses x's sign when
// x is ±0 because IEEE 754 sums (-0)+(+0) → +0).
const SLONG_MIN: bigint = -(1n << 63n);
const SLONG_MAX: bigint = (1n << 63n) - 1n;

/**
 * Bit length of a non-negative bigint. Same shape as set_z.ts /
 * set_z_2exp's helper.
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
  // x is validated by mpfr_add downstream.
  void x;
}

export function mpfr_add_z(
  x: MPFR,
  z: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(x, z, prec, rnd);

  // Mirror C dispatch (mpfr/src/gmp_op.c L106-L112): if z fits in slong,
  // delegate to mpfr_add_si. Otherwise lift z losslessly via mpfr_set_z
  // and call mpfr_add.
  if (z >= SLONG_MIN && z <= SLONG_MAX) {
    return mpfr_add_si(x, z, prec, rnd);
  }
  const absZ = z < 0n ? -z : z;
  const intPrec = bitLength(absZ);
  const tPrec = intPrec < PREC_MIN ? PREC_MIN : intPrec;
  const t = mpfr_set_z(z, tPrec, 'RNDN').value;
  return mpfr_add(x, t, prec, rnd);
}
