/**
 * reference_ports/broken/mpfr_set_si.ts — deliberately-buggy mpfr_set_si.
 *
 * **Deliberately broken: drops the sign AND ignores the magnitude on
 * inexact rounding paths** — always rounds toward zero by truncation
 * regardless of `rnd`, and always returns sign +1 (even when n was
 * negative). Bug shape: a plausible "I forgot to handle several
 * orthogonal concerns" mistake — sign tracking missed AND rounding
 * mode ignored.
 *
 * The two-axis breakage is intentional: dropping only the sign would
 * leave the broken port passing on all-positive cases (~50% of the
 * golden by count), missing the ≤ 0.5 composite gate. Compounding the
 * sign bug with a rounding-mode bug brings the failure rate above
 * the gate threshold across the full golden.
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: must score composite
 * ≤ 0.5.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: src/ops/set_si.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

const LONG_MIN_VAL: bigint = -(1n << 63n);
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

function bitLength(n: bigint): bigint {
  let bits = 0n;
  let probe = n;
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

function validateArgs(n: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < LONG_MIN_VAL || n > LONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of int64 range [${LONG_MIN_VAL}, ${LONG_MAX_VAL}], got ${n}`,
    );
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

export function mpfr_set_si(
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(n, prec, rnd);

  if (n === 0n) {
    return { value: posZero(prec), ternary: 0 };
  }

  // BUG 1: should use sign = n < 0n ? -1 : 1; always positive.
  const sign: Sign = 1;
  const absN: bigint = n < 0n ? -n : n;

  const srcPrec = bitLength(absN);
  const srcMant = absN;
  const srcExp = srcPrec;

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

  // BUG 2: should pass `rnd`; always uses 'RNDZ' (truncation).
  void rnd;
  const { mant, exp, ternary } = roundMantissa(
    srcMant,
    srcPrec,
    srcExp,
    prec,
    sign,
    'RNDZ',
  );
  const value: MPFR = { kind: 'normal', sign, prec, exp, mant };
  return { value, ternary };
}
