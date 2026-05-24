/**
 * reference_ports/broken/mpfr_check_range.ts — deliberately-buggy
 * mpfr_check_range.
 *
 * **Multi-bug perturbation (all targeting the in-range path the golden
 * exercises):**
 *   1. Negates ternary (returns -t instead of t). Every case with t != 0
 *      fails on ternary mismatch. Cases with t == 0 still pass that
 *      branch.
 *   2. For NaN inputs: returns posZero(53n) instead of (x, t). Breaks
 *      the NaN-passthrough cases.
 *   3. For ±0: swaps the sign in the returned value (sign-flip).
 *
 * NOT used in production.
 *
 * Ref: src/ops/check_range.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import { MPFRError, posZero } from '../../../src/core.ts';

function isTernary(t: unknown): t is Ternary {
  return t === -1 || t === 0 || t === 1;
}

export function mpfr_check_range(
  x: MPFR,
  t: Ternary,
  rnd: RoundingMode,
): Result {
  if (!isTernary(t)) throw new MPFRError('EPREC', `bad t`);
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `bad rnd`);
  }

  // BUG 2: NaN → posZero.
  if (x.kind === 'nan') {
    return { value: posZero(53n), ternary: 0 };
  }

  // BUG 3: zero with flipped sign.
  if (x.kind === 'zero') {
    const flipped: MPFR = { ...x, sign: (-x.sign) as Sign };
    // BUG 1: negate ternary.
    const nt: Ternary = (t === 0 ? 0 : t === 1 ? -1 : 1) as Ternary;
    return { value: flipped, ternary: nt };
  }

  // BUG 1: negate ternary for all other in-range cases.
  const nt: Ternary = (t === 0 ? 0 : t === 1 ? -1 : 1) as Ternary;
  return { value: x, ternary: nt };
}
