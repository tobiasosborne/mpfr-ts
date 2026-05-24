/**
 * reference_ports/broken/mpfr_set.ts — deliberately-buggy mpfr_set.
 *
 * **Multi-bug perturbation:**
 *   1. For `kind === 'normal'` with `prec < b.prec`, pass `-b.sign` (the
 *      opposite of the source sign) to roundMantissa — this inverts
 *      the rounding direction for RNDU/RNDD and silently miscomputes
 *      ternary in many cases.
 *   2. For `kind === 'zero'`, always return `+0` (drops sign).
 *   3. For `kind === 'inf'`, always return `+inf` (drops sign).
 *   4. For NaN, return a "nan-like" value with sign=-1 (violates schema
 *      validate(), so the harness will treat these as throws —
 *      conservatively, this still flips coverage).
 *
 * Composite should drop well below 0.45 — multi-bug perturbations
 * hit every kind branch and at least half of the rounding-mode/sign
 * combinations.
 *
 * NOT used in production.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/set.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posInf,
  posZero,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

function validateArgs(prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec out of range: ${prec}`);
  }
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

export function mpfr_set(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // BUG 4: NaN-like return with wrong shape (caught by schema).
  if (b.kind === 'nan') {
    return {
      value: { kind: 'nan', sign: 1, prec: 0n, exp: 0n, mant: 0n },
      ternary: 0,
    };
  }

  // BUG 2 & 3: drop sign for zero/inf.
  if (b.kind === 'inf') {
    return { value: posInf(prec), ternary: 0 };
  }
  if (b.kind === 'zero') {
    return { value: posZero(prec), ternary: 0 };
  }

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_set(broken): unexpected kind`);
  }

  if (prec === b.prec) {
    // BUG: flip sign + bump exp.
    return {
      value: {
        kind: 'normal',
        sign: (-b.sign) as Sign,
        prec,
        exp: b.exp + 1n,
        mant: b.mant,
      },
      ternary: 0,
    };
  }

  if (prec > b.prec) {
    // BUG: shift mantissa wrong direction (right-shift instead of left), flip sign.
    const padShift = prec - b.prec;
    return {
      value: {
        kind: 'normal',
        sign: (-b.sign) as Sign,
        prec,
        exp: b.exp - padShift,  // wrong direction
        mant: 1n << (prec - 1n),  // just MSB
      },
      ternary: 1,
    };
  }

  // BUG 1: invert sign for rounding direction. RNDU/RNDD flip.
  const invSign: Sign = (-b.sign) as Sign;
  const { mant, exp, ternary } = roundMantissa(
    b.mant,
    b.prec,
    b.exp,
    prec,
    invSign,
    rnd,
  );
  // Also flip sign and ternary in returned value.
  const flippedTernary: Ternary = (ternary === 0 ? 0 : ternary === 1 ? -1 : 1) as Ternary;
  return {
    value: { kind: 'normal', sign: (-b.sign) as Sign, prec, exp, mant },
    ternary: flippedTernary,
  };
}
