/**
 * reference_ports/broken/mpfr_buildopt_sharedcache_p.ts -- deliberately-buggy
 * mpfr_buildopt_sharedcache_p.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_buildopt_sharedcache_p golden, the golden is too weak.
 *
 * **Deliberately broken: returns `true` instead of `false`.** This is
 * the strongest possible perturbation for a no-arg boolean predicate:
 * the value is inverted. The golden's single happy case (expected
 * `false`) MUST fail, dropping composite to 0.0.
 *
 * Mutation-prove gap: correct port scores 1.0 (1/1 pass); broken port
 * scores 0.0 (0/1 pass) -- a clean 1.0 gap, far outside the 0.45-0.55
 * danger zone.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 -- mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 -- composite must drop below 0.55 under mutation.
 * Ref: eval/reference_ports/correct/mpfr_buildopt_sharedcache_p.ts -- correct version.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_buildopt_sharedcache_p(): boolean {
  // BUG: should be `false` (no shared-cache infra in TS port).
  // Returning the opposite literal so the single happy case fails on the wire.
  return true;
}
