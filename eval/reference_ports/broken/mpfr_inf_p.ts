/**
 * reference_ports/broken/mpfr_inf_p.ts — deliberately-buggy mpfr_inf_p.
 *
 * **Deliberately broken: returns the *negation* of the correct
 * predicate — `kind !== 'inf'` instead of `kind === 'inf'`.** Mirror
 * of the `nan_p` broken: a polarity flip every case answer.
 *
 * Behaviour vs. correct port: every case flips.
 *
 *   - Inf  : correct true,  broken false (FAIL)
 *   - NaN  : correct false, broken true  (FAIL)
 *   - zero : correct false, broken true  (FAIL)
 *   - norm : correct false, broken true  (FAIL)
 *
 * Every case fails; composite → 0. See nan_p broken for why polarity
 * flip is preferred over kind-confusion here.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/inf_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_inf_p(x: MPFR): boolean {
  // BUG: polarity flip.
  return x.kind !== 'inf';
}
