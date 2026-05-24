/**
 * reference_ports/broken/mpfr_sub.ts — deliberately-buggy mpfr_sub.
 *
 * Used to mutation-prove the golden master per CLAUDE.md PIL.3.
 *
 * **Deliberately broken: returns `{value: a, ternary: 0}` regardless
 * of b, prec, rnd.** Ignores the subtrahend entirely. Mirrors the
 * symmetric "ignore b" bug in broken/mpfr_add.ts. Every nonzero
 * subtraction produces the wrong value; every NaN-on-b case produces
 * a non-NaN result; every signed-zero rounding edge fails; precision
 * mismatch on (prec != a.prec) cases adds further breakage.
 *
 * NOT used in production. NOT imported from src/. Do NOT fix.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/sub.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';

export function mpfr_sub(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // BUG: ignore b, prec, rnd. Return a as-is with ternary 0.
  void b;
  void prec;
  void rnd;
  return { value: a, ternary: 0 };
}
