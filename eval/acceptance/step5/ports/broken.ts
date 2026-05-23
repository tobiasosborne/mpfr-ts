/**
 * Step-5 acceptance port (b): structurally valid but wrong-ternary.
 *
 * Identical to ports/correct.ts except `ternary` is `1` instead of `0`.
 * The value half matches the golden's expected MPFR exactly; only the
 * ternary flag is wrong. This exercises the runner's ternary-direction
 * check (CLAUDE.md "Hallucination-risk callouts: Ternary flag is the
 * sign of (rounded - exact), not 0/1") — a port that drops the sign
 * convention is wrong even when the value half is right.
 *
 * Expected grade: composite < 0.5 on the same golden as scenario (a).
 *
 * RED-phase scaffolding. Runner not yet implemented.
 */

import type { MPFR, Result, Ternary } from '../../../../src/core.ts';

/**
 * Wrong-ternary identity. Returns the input value unchanged but reports
 * `ternary = 1` ("rounded > exact") for every case. Since the runner's
 * goldens carry `ternary: 0` (the value is unchanged, so exact), every
 * case should fail comparison.
 */
export function acceptanceFn(x: MPFR): Result {
  const ternary: Ternary = 1;
  return { value: x, ternary };
}
