/**
 * reference_ports/correct/mpfr_cmp.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/cmp.ts`. This wrapper exists so the harness can run
 * identical commands against `correct/<fn>.ts` and `broken/<fn>.ts`
 * by changing only the directory prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence between this
 * file and `src/ops/cmp.ts` is a source-of-truth bug: the production
 * op IS the reference. If the reference needs to change, change the
 * production file.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

// `import type` keeps the schema dependence visible to the ast_check
// scanner (which requires a core.ts import for non-substrate ports)
// while remaining a zero-runtime statement. The `mpfr_cmp` import is
// the actual delegation; see `src/ops/cmp.ts` for the algorithm.
import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp as _impl } from '../../../src/ops/cmp.ts';

export function mpfr_cmp(a: MPFR, b: MPFR): number {
  return _impl(a, b);
}
