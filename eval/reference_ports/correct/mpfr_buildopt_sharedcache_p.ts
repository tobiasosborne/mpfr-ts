/**
 * reference_ports/correct/mpfr_buildopt_sharedcache_p.ts -- mutation-prove
 * reference for mpfr_buildopt_sharedcache_p.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline for the golden master. It is self-contained
 * (the production src/ops/buildopt_sharedcache_p.ts does not yet exist;
 * the orchestrator will materialise it during the port-and-grade flow)
 * and returns the same compile-time constant `false` documented in
 * eval/functions/mpfr_buildopt_sharedcache_p/spec.json.
 *
 * The type-only core.ts import satisfies the ast_check gate
 * (CLAUDE.md Law 4) without affecting the predicate's value.
 *
 * Ref: mpfr/src/buildopt.c L86-L94 -- C reference.
 * Ref: eval/functions/mpfr_buildopt_sharedcache_p/spec.json -- contract.
 * Ref: src/ops/buildopt_bfloat16_p.ts -- sibling template.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_buildopt_sharedcache_p(): boolean {
  // Pure-TS has no shared-cache infrastructure; per CLAUDE.md Rule 4
  // the per-test Worker isolation model would preclude one anyway.
  // Returning false is the only honest answer. See spec.json
  // divergence_from_c.
  return false;
}
