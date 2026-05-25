/**
 * reference_ports/broken/mpfr_nextabove.ts -- deliberately-buggy mpfr_nextabove.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_nextabove golden, the golden is too weak.
 *
 * **Deliberately broken: dispatch table is SWAPPED with mpfr_nextbelow.**
 * Negative x routes to nexttoinf (instead of nexttozero); non-negative x
 * routes to nexttozero (instead of nexttoinf). This is exactly the agent
 * error the spec.json's "ASYMMETRY between nextabove and nextbelow" note
 * warns about -- an agent copying the body of nextbelow into nextabove
 * (or vice versa) without flipping the dispatch ships a port that
 * produces the OPPOSITE direction of stepping on every finite case.
 *
 * Expected failure surface:
 *   - happy: every finite case fails (output is one step toward -infinity
 *     instead of +infinity).
 *   - edge: +/-Inf cases produce wrong outputs (e.g. +Inf -> setmax
 *     finite instead of staying +Inf); +/-0 cases produce -smallest
 *     instead of +smallest (IEEE nextDown shape instead of nextUp).
 *   - adversarial, fuzz: full failures.
 *   - NaN cases still pass (NaN propagation isn't affected by dispatch swap).
 *
 * Mutation-prove gap: correct port -> 1.0; broken port -> ~0.04 (only
 * the 3 NaN edge cases pass out of 22+34+12+55+6 = 129 total). Well
 * outside the 0.45-0.55 danger zone.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 -- mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 -- composite must drop below 0.55 under mutation.
 * Ref: eval/reference_ports/correct/mpfr_nextabove.ts -- correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { NAN_VALUE } from '../../../src/core.ts';
import { mpfr_nexttozero } from '../../../src/ops/nexttozero.ts';
import { mpfr_nexttoinf } from '../../../src/ops/nexttoinf.ts';

export function mpfr_nextabove(x: MPFR): MPFR {
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }
  // BUG: dispatch swapped -- this is the nextbelow dispatch table. Should
  // route negative -> nexttozero, non-negative -> nexttoinf.
  if (x.sign === -1) {
    return mpfr_nexttoinf(x);
  }
  return mpfr_nexttozero(x);
}
