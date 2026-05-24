/**
 * reference_ports/broken/mpfr_neg.ts — deliberately-buggy mpfr_neg.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.5 on
 * the mpfr_neg golden, the golden is too weak.
 *
 * **Deliberately broken: forgets to flip the sign.** Every code path
 * that should produce a sign change instead returns the operand
 * unmodified (after a prec-rounding step, so non-sign properties are
 * still computed correctly). The bug shape mirrors a plausible agent
 * error: "I copied mpfr_set's structure, not mpfr_neg's."
 *
 * Behaviour:
 *   - NaN → NaN (matches correct).
 *   - ±Inf → ±Inf with the ORIGINAL sign (bug: should flip).
 *   - ±0 → ±0 with the ORIGINAL sign (bug: should flip).
 *   - normal → x rounded to prec with the ORIGINAL sign (bug: should
 *     flip; also note that without the sign flip the ternary direction
 *     computed against the original sign happens to match the value's
 *     direction-of-rounding, so ternary is "consistent with the bug"
 *     and the wrongness shows up as a sign mismatch, not a ternary
 *     mismatch — making the bug correctness-real, not cosmetic).
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/neg.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
} from '../../../src/core.ts';
import { roundMantissa } from '../../../src/internal/mpfr/round_raw.ts';

function validateArgs(prec: bigint, rnd: RoundingMode): void {
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

export function mpfr_neg(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  validateArgs(prec, rnd);

  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // BUG: should flip sign for ±Inf. Preserves original.
  if (x.kind === 'inf') {
    return {
      value: x.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // BUG: should flip sign for ±0. Preserves original.
  if (x.kind === 'zero') {
    return {
      value: x.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  if (x.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_neg(broken): unexpected kind ${String(x.kind)}`);
  }

  // BUG: should use newSign = -x.sign. Uses x.sign.
  const sign = x.sign;

  if (prec >= x.prec) {
    const padShift = prec - x.prec;
    const value: MPFR = {
      kind: 'normal',
      sign,
      prec,
      exp: x.exp,
      mant: x.mant << padShift,
    };
    return { value, ternary: 0 };
  }

  const { mant, exp, ternary } = roundMantissa(
    x.mant,
    x.prec,
    x.exp,
    prec,
    sign,
    rnd,
  );
  const value: MPFR = { kind: 'normal', sign, prec, exp, mant };
  return { value, ternary };
}
