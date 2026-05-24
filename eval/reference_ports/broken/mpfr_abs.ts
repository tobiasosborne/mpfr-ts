/**
 * reference_ports/broken/mpfr_abs.ts — deliberately-buggy mpfr_abs.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.5 on
 * the mpfr_abs golden, the golden is too weak.
 *
 * **Deliberately broken: returns `-|x|` instead of `|x|`.** Every
 * non-NaN branch fixes the result sign to `-1` instead of `+1`. The bug
 * shape mirrors a plausible agent error: "I confused abs and neg in the
 * mpfr_set4 sign argument."
 *
 * Behaviour:
 *   - NaN → NaN (matches correct).
 *   - ±Inf → -Inf instead of +Inf.
 *   - ±0 → -0 instead of +0.
 *   - normal → magnitude rounded to prec, with sign forced to -1.
 *     Since the rounding step uses the new (wrong) sign for the
 *     direction lookup, ternary is computed consistently with the
 *     buggy sign — so the failure shows up as a sign mismatch on every
 *     non-NaN case.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: src/ops/abs.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
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

export function mpfr_abs(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  validateArgs(prec, rnd);

  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // BUG: should be posInf. Returns negInf.
  if (x.kind === 'inf') {
    return { value: negInf(prec), ternary: 0 };
  }

  // BUG: should be posZero. Returns negZero.
  if (x.kind === 'zero') {
    return { value: negZero(prec), ternary: 0 };
  }

  if (x.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_abs(broken): unexpected kind ${String(x.kind)}`);
  }

  if (prec >= x.prec) {
    const padShift = prec - x.prec;
    const value: MPFR = {
      kind: 'normal',
      // BUG: should be 1. Use -1.
      sign: -1,
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
    // BUG: should pass 1. Pass -1.
    -1,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    // BUG: should be 1. Use -1.
    sign: -1,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
