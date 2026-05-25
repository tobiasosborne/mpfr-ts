/**
 * reference_ports/correct/mpfr_nextabove.ts -- mutation-prove reference
 * for mpfr_nextabove.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline for the golden master. The production
 * src/ops/nextabove.ts does not yet exist; the orchestrator will
 * materialise it during the port-and-grade flow. This file is
 * self-contained but DELEGATES to the existing src/ops/nexttozero.ts
 * and src/ops/nexttoinf.ts (which are already shipped at composite
 * 1.0) -- the 11-line C body of mpfr_nextabove is pure dispatch and
 * the algorithm lives entirely in those two delegates.
 *
 * Algorithm (mpfr/src/next.c L119-L131):
 *   - NaN -> propagate (C sets MPFR_FLAGS_NAN and leaves x; TS returns NAN_VALUE).
 *   - Negative x -> mpfr_nexttozero(x): toward zero is "above" for x<0.
 *   - Non-negative x (incl. +0, -0) -> mpfr_nexttoinf(x).
 *
 * Signed-zero asymmetry: nextabove(+0) routes via nexttoinf -> +smallest;
 * nextabove(-0) routes via nexttozero -> +smallest (sign flip). Both
 * yield +smallest, matching IEEE 754 nextUp(+/-0).
 *
 * Ref: mpfr/src/next.c L119-L131 -- C reference.
 * Ref: src/ops/nexttozero.ts -- negative-x delegate (shipped).
 * Ref: src/ops/nexttoinf.ts -- non-negative-x delegate (shipped).
 * Ref: eval/functions/mpfr_nextabove/spec.json -- contract.
 */

import type { MPFR } from '../../../src/core.ts';
import { NAN_VALUE } from '../../../src/core.ts';
import { mpfr_nexttozero } from '../../../src/ops/nexttozero.ts';
import { mpfr_nexttoinf } from '../../../src/ops/nexttoinf.ts';

export function mpfr_nextabove(x: MPFR): MPFR {
  // (1) NaN: propagate. The C sets a global flag (MPFR_FLAGS_NAN) we don't model.
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }
  // (2) Dispatch by sign. MPFR_IS_NEG inspects the sign field directly,
  //     so -0 routes through nexttozero (which sign-flips back to +).
  if (x.sign === -1) {
    return mpfr_nexttozero(x);
  }
  return mpfr_nexttoinf(x);
}
