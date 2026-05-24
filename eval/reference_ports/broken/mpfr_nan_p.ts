/**
 * reference_ports/broken/mpfr_nan_p.ts — deliberately-buggy mpfr_nan_p.
 *
 * **Deliberately broken: returns the *negation* of the correct
 * predicate — `kind !== 'nan'` instead of `kind === 'nan'`.** A
 * plausible "agent inverted the intent" mutation — the predicate still
 * has the right shape but answers backwards.
 *
 * Behaviour vs. correct port: every case flips.
 *
 *   - NaN  : correct true,  broken false (FAIL)
 *   - Inf  : correct false, broken true  (FAIL)
 *   - zero : correct false, broken true  (FAIL)
 *   - norm : correct false, broken true  (FAIL)
 *
 * Every case fails; composite → 0. Comfortably below the 0.5 ceiling.
 *
 * Rationale for picking *this* mutation over the kind-confused variant
 * (`kind === 'inf'`): the kind-confused variant agrees with the
 * correct port on every zero-input and normal-input case (both return
 * false), which dominates a balanced predicate input set — the broken
 * port then scored composite ≈ 0.61 on a balanced golden, breaching the
 * 0.5 ceiling. The polarity flip is both a more plausible agent error
 * (inverted intent vs. typo'd discriminant) AND a stronger mutation,
 * so we adopt it. The golden retains the balanced input distribution.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/nan_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_nan_p(x: MPFR): boolean {
  // BUG: polarity flip. Correct returns x.kind === 'nan'; we return
  // the negation.
  return x.kind !== 'nan';
}
