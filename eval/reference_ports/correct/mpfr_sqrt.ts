/**
 * reference_ports/correct/mpfr_sqrt.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/sqrt.ts`. This wrapper exists so the harness can run identical
 * commands against `correct/<fn>.ts` and `broken/<fn>.ts` by changing
 * only the directory prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence between this
 * file and `src/ops/sqrt.ts` is a source-of-truth bug.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_sqrt as _impl } from '../../../src/ops/sqrt.ts';

export function mpfr_sqrt(
  x: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(x, prec, rnd);
}
