/**
 * reference_ports/correct/mpfr_get_d.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/get_d.ts`. This wrapper exists so the harness can run
 * identical commands against `correct/<fn>.ts` and `broken/<fn>.ts`
 * by changing only the directory prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence between this
 * file and `src/ops/get_d.ts` is a source-of-truth bug: the production
 * op IS the reference. If the reference needs to change, change the
 * production file.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

// `import type` keeps the schema dependence visible to the ast_check
// scanner (which requires a core.ts import for non-substrate ports)
// while remaining a zero-runtime statement. The `mpfr_get_d` import is
// the actual delegation; see `src/ops/get_d.ts` for the algorithm.
import type { MPFR, RoundingMode } from '../../../src/core.ts';
import { mpfr_get_d as _impl } from '../../../src/ops/get_d.ts';

export function mpfr_get_d(x: MPFR, rnd: RoundingMode): number {
  return _impl(x, rnd);
}
