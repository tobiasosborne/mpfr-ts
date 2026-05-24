/**
 * reference_ports/broken/mpfr_sqrt.ts — deliberately-buggy mpfr_sqrt.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md Step 8
 * and CLAUDE.md PIL.3 ("perturb the reference port, confirm the
 * composite drops below 0.95").
 *
 * **Deliberately broken: every input returns `{value: x, ternary: 0}`
 * — i.e. just returns the operand at its native precision, ignoring
 * `prec` and `rnd` entirely.**
 *
 * The bug is total: every sqrt computes `x` instead of `sqrt(x)`; every
 * negative-input case produces a non-NaN result; every NaN-on-input
 * case produces a non-NaN result; the precision of the result is
 * `x.prec` not the requested `prec`. Even sqrt(1) (which would be 1
 * by happy accident) fails because the returned prec is wrong.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/sqrt.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';

export function mpfr_sqrt(
  x: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // BUG: ignore prec and rnd entirely; return x as-is and ternary 0.
  void prec;
  void rnd;
  return { value: x, ternary: 0 };
}
