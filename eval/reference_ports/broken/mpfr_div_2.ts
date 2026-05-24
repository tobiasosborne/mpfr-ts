/**
 * reference_ports/broken/mpfr_div_2.ts -- deliberately-buggy mpfr_div_2.
 *
 * **Multi-bug perturbation (per worklog 006 #6 -- single-bug perturbations
 * land in the 0.45-0.55 mutation-prove danger zone; multi-bug pushes
 * cleanly below 0.30):**
 *
 *   1. Swaps operands -- computes v / u instead of u / v. Wrong on every
 *      case where u != v, including the happy 6.0/2.0 -> 0.333... vs 3.0
 *      mismatch.
 *
 *   2. Inverts directional rounding modes: RNDU swaps with RNDD. This
 *      compounds with bug #1 on the edge cases that exercise both
 *      polarities.
 *
 * Expected mutation-prove score: composite well below 0.30 on the
 * generated golden -- bug #1 alone breaks 100% of cases where u != v;
 * bug #2 adds breakage on the inexact-RNDU/D cases.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/div_2.ts -- the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_div } from '../../../src/ops/div.ts';

function invertRnd(rnd: RoundingMode): RoundingMode {
  if (rnd === 'RNDU') return 'RNDD';
  if (rnd === 'RNDD') return 'RNDU';
  return rnd;
}

export function mpfr_div_2(
  u: MPFR, v: MPFR, prec: bigint, rnd: RoundingMode,
): Result {
  // BUG 1: swap operands -- compute v / u, not u / v.
  // BUG 2: invert directional rounding.
  return mpfr_div(v, u, prec, invertRnd(rnd));
}
