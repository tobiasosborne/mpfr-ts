/**
 * reference_ports/correct/mpfr_neg.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/neg.ts`. This wrapper exists so the harness can run identical
 * commands against `correct/<fn>.ts` and `broken/<fn>.ts` by changing
 * only the directory prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence is a
 * source-of-truth bug: the production op IS the reference.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

// `import type` keeps the schema dependence visible to the ast_check
// scanner while remaining zero-runtime; the `mpfr_neg` import is the
// actual delegation. See `src/ops/neg.ts` for the algorithm.
import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_neg as _impl } from '../../../src/ops/neg.ts';

export function mpfr_neg(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  return _impl(x, prec, rnd);
}
