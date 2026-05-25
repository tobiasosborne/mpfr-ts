/**
 * reference_ports/correct/mpfr_nextbelow.ts -- mutation-prove reference
 * for mpfr_nextbelow.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline for the golden master. The production
 * src/ops/nextbelow.ts does not yet exist; the orchestrator will
 * materialise it during the port-and-grade flow. This file is
 * self-contained but DELEGATES to the existing src/ops/nexttozero.ts
 * and src/ops/nexttoinf.ts -- the 15-line C body of mpfr_nextbelow is
 * pure dispatch and the algorithm lives entirely in those delegates.
 * The only difference from mpfr_nextabove is the dispatch table:
 * negative-x routes to nexttoinf (more negative), non-negative-x to
 * nexttozero (toward zero).
 *
 * Algorithm (mpfr/src/next.c L133-L147):
 *   - NaN -> propagate.
 *   - Negative x -> mpfr_nexttoinf(x): further from zero is "below" for x<0.
 *   - Non-negative x (incl. +0, -0) -> mpfr_nexttozero(x).
 *
 * Signed-zero asymmetry: nextbelow(+0) routes via nexttozero -> -smallest
 * (sign flip); nextbelow(-0) routes via nexttoinf -> -smallest. Both
 * yield -smallest, matching IEEE 754 nextDown(+/-0).
 *
 * Ref: mpfr/src/next.c L133-L147 -- C reference.
 * Ref: src/ops/nexttoinf.ts -- negative-x delegate (shipped).
 * Ref: src/ops/nexttozero.ts -- non-negative-x delegate (shipped).
 * Ref: eval/functions/mpfr_nextbelow/spec.json -- contract.
 */

import type { MPFR } from '../../../src/core.ts';
import { NAN_VALUE } from '../../../src/core.ts';
import { mpfr_nexttozero } from '../../../src/ops/nexttozero.ts';
import { mpfr_nexttoinf } from '../../../src/ops/nexttoinf.ts';

export function mpfr_nextbelow(x: MPFR): MPFR {
  // (1) NaN: propagate.
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }
  // (2) Dispatch by sign -- inverted relative to nextabove.
  if (x.sign === -1) {
    return mpfr_nexttoinf(x);
  }
  return mpfr_nexttozero(x);
}
