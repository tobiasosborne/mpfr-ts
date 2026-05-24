/**
 * reference_ports/broken/mpfr_set4.ts — deliberately-buggy mpfr_set4.
 *
 * **Deliberately broken: use `b.sign` (not `signb`) as the rounding
 * direction in the lossy-prec branch.** This is the classic subtle bug
 * the C reference avoids by parameterising MPFR_RNDRAW with `signb`.
 *
 * Effect: when `prec < b.prec` AND `signb !== b.sign` AND the rounding
 * mode is sign-asymmetric (RNDU or RNDD), the rounded mantissa goes the
 * wrong way and the ternary is inverted.
 *
 * Also: ALWAYS preserve b's sign even for non-normal kinds (zero/inf),
 * so all signb-flipping cases break for non-normal kinds too.
 *
 * Composite should drop well below 0.55 since the broken behaviour
 * affects every case where signb !== b.sign (≈ 1/2 of the fuzz pool)
 * AND every Inf/Zero case with non-matching signb (deterministic
 * miscompare).
 *
 * NOT used in production.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/set4.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../../../src/core.ts';
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

function validateArgs(prec: bigint, rnd: RoundingMode, signb: Sign): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec out of range: ${prec}`);
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
  if (signb !== 1 && signb !== -1) {
    throw new MPFRError('EPREC', `signb must be 1 or -1, got ${String(signb)}`);
  }
}

export function mpfr_set4(
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
  signb: Sign,
): Result {
  validateArgs(prec, rnd, signb);

  if (b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // BUG: ignore signb for inf/zero; use b.sign instead.
  if (b.kind === 'inf') {
    return {
      value: b.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  if (b.kind === 'zero') {
    return {
      value: b.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  if (b.kind !== 'normal') {
    throw new MPFRError('EPREC', `mpfr_set4(broken): unexpected kind`);
  }

  if (prec === b.prec) {
    // BUG: keep b.sign instead of using signb on the same-prec exact path.
    const value: MPFR = {
      kind: 'normal',
      sign: b.sign,
      prec,
      exp: b.exp,
      mant: b.mant,
    };
    return { value, ternary: 0 };
  }

  if (prec > b.prec) {
    // BUG: keep b.sign on the lossless pad path.
    const padShift = prec - b.prec;
    const value: MPFR = {
      kind: 'normal',
      sign: b.sign,
      prec,
      exp: b.exp,
      mant: b.mant << padShift,
    };
    return { value, ternary: 0 };
  }

  // BUG: pass b.sign instead of signb to roundMantissa. AND attach b.sign,
  // not signb, to the returned value mant/exp — so the lossy path also
  // forgets the signb override entirely.
  const { mant, exp, ternary } = roundMantissa(
    b.mant,
    b.prec,
    b.exp,
    prec,
    b.sign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign: b.sign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
