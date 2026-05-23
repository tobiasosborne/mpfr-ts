/**
 * reference_ports/broken/mpfr_add.ts — deliberately-buggy mpfr_add.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md Step 8
 * and CLAUDE.md PIL.3 ("perturb the reference port, confirm the
 * composite drops below 0.95"). This file is the executable assertion
 * that the golden distinguishes correct from subtly-incorrect behaviour.
 *
 * **Deliberately broken: every input returns `{value: a, ternary: 0}`
 * — i.e. just returns the first operand at its native precision,
 * ignoring `b` and `rnd` entirely.**
 *
 * The bug is total: every nonzero add produces the wrong value (a
 * instead of a+b); every NaN-on-b case produces a non-NaN result;
 * every signed-zero rounding edge produces the wrong sign; the
 * precision of the result is `a.prec` not the requested `prec` (so
 * even cases where `a + b == a` mathematically fail on prec mismatch).
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/add.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';

export function mpfr_add(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // BUG: ignore b and rnd entirely; return a as-is (even with its own
  // prec, not the requested prec) and ternary 0.
  void b;
  void prec;
  void rnd;
  return { value: a, ternary: 0 };
}
