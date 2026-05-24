/**
 * reference_ports/broken/mpfr_signbit.ts — deliberately-buggy mpfr_signbit.
 *
 * **Deliberately broken: returns `x.sign === 1` instead of
 * `x.sign === -1`.** A polarity-flip mutation — every case answer is
 * negated.
 *
 * Behaviour vs. correct port: every case flips.
 *
 *   - +n / +0 / +Inf / NaN : correct false, broken true  (FAIL)
 *   - -n / -0 / -Inf       : correct true,  broken false (FAIL)
 *
 * Every case fails; composite → 0. Comfortably below the 0.5 ceiling.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/signbit.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_signbit(x: MPFR): boolean {
  // BUG: polarity flip. Should be sign === -1, is sign === 1.
  return x.sign === 1;
}
