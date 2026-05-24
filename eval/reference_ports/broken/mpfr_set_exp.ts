/**
 * reference_ports/broken/mpfr_set_exp.ts — deliberately-buggy mpfr_set_exp.
 *
 * **Multi-bug perturbation:**
 *   1. For kind='normal': returns x with exp = -e instead of exp = e
 *      (sign-flipped exponent). Every case where e != 0 fails on exp
 *      mismatch; e = 0 cases still pass.
 *   2. For kind='normal': flips x.sign in the returned value
 *      ({-1, 1} swap). Every case fails on sign mismatch.
 *   3. For non-normal kinds: returns x unchanged instead of throwing.
 *      No effect on this golden (which only has normal inputs).
 *
 * NOT used in production.
 *
 * Ref: src/ops/set_exp.ts — the correct version.
 */

import type { MPFR, Sign } from '../../../src/core.ts';

export function mpfr_set_exp(x: MPFR, e: bigint): MPFR {
  // BUG 3 (latent): doesn't throw for non-normal kinds.
  if (x.kind !== 'normal') return x;
  // BUG 1 + 2: wrong exp direction, flipped sign.
  return {
    kind: 'normal',
    sign: (-x.sign) as Sign,
    prec: x.prec,
    exp: -e,
    mant: x.mant,
  };
}
