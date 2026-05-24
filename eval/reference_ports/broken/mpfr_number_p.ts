/**
 * reference_ports/broken/mpfr_number_p.ts — deliberately-buggy mpfr_number_p.
 *
 * **Deliberately broken: returns the *negation* of the correct
 * predicate — `kind === 'inf' || kind === 'nan'` instead of
 * `kind === 'normal' || kind === 'zero'`.** A plausible "agent inverted
 * the polarity" mutation — picks the two excluded kinds instead of
 * the two included.
 *
 * Behaviour vs. correct port: every case flips.
 *
 *   - norm : correct true,  broken false (FAIL)
 *   - zero : correct true,  broken false (FAIL)
 *   - Inf  : correct false, broken true  (FAIL)
 *   - NaN  : correct false, broken true  (FAIL)
 *
 * Every case fails; composite → 0. Comfortably below the 0.5 ceiling.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/number_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_number_p(x: MPFR): boolean {
  // BUG: polarity flip. Should be 'normal' | 'zero', is 'inf' | 'nan'.
  return x.kind === 'inf' || x.kind === 'nan';
}
