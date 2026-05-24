/**
 * reference_ports/broken/mpfr_div.ts — deliberately-buggy mpfr_div.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md Step 8
 * and CLAUDE.md PIL.3 ("perturb the reference port, confirm the
 * composite drops below 0.95").
 *
 * **Deliberately broken: every input returns `{value: a, ternary: 0}`
 * — i.e. just returns the first operand at its native precision,
 * ignoring `b`, `prec`, and `rnd` entirely.**
 *
 * The bug is total: every division computes `a` instead of `a/b`; every
 * NaN-on-`b` case produces a non-NaN result; every signed-zero
 * sign-product edge produces the wrong sign; every Inf/Inf or 0/0 case
 * produces a non-NaN result; every divbyzero case (finite/0) produces
 * a normal value instead of Inf. The precision of the result is
 * `a.prec` not the requested `prec`.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/div.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';

export function mpfr_div(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // BUG: ignore b, prec, and rnd entirely; return a as-is and ternary 0.
  void b;
  void prec;
  void rnd;
  return { value: a, ternary: 0 };
}
