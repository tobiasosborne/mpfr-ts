/**
 * reference_ports/broken/mpfr_nextbelow.ts -- deliberately-buggy mpfr_nextbelow.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_nextbelow golden, the golden is too weak.
 *
 * **Deliberately broken: dispatch table is SWAPPED with mpfr_nextabove.**
 * Negative x routes to nexttozero (instead of nexttoinf); non-negative x
 * routes to nexttoinf (instead of nexttozero). Mirror of the broken
 * nextabove. An agent that copies one of the two bodies without
 * flipping dispatch produces this exact bug -- and the goldens for both
 * functions catch it independently.
 *
 * Expected failure surface: same shape as broken nextabove -- every
 * finite case steps in the WRONG direction (toward +infinity instead
 * of -infinity). NaN cases still pass.
 *
 * Mutation-prove gap: correct port -> 1.0; broken port -> ~0.04. Well
 * outside the 0.45-0.55 danger zone.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 -- mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 -- composite must drop below 0.55 under mutation.
 * Ref: eval/reference_ports/correct/mpfr_nextbelow.ts -- correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { NAN_VALUE } from '../../../src/core.ts';
import { mpfr_nexttozero } from '../../../src/ops/nexttozero.ts';
import { mpfr_nexttoinf } from '../../../src/ops/nexttoinf.ts';

export function mpfr_nextbelow(x: MPFR): MPFR {
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }
  // BUG: dispatch swapped -- this is the nextabove dispatch table. Should
  // route negative -> nexttoinf, non-negative -> nexttozero.
  if (x.sign === -1) {
    return mpfr_nexttozero(x);
  }
  return mpfr_nexttoinf(x);
}
