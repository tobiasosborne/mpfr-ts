/**
 * reference_ports/correct/mpfr_add.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/add.ts`. This wrapper exists so the harness can run identical
 * commands against `correct/<fn>.ts` and `broken/<fn>.ts` by changing
 * only the directory prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence between this
 * file and `src/ops/add.ts` is a source-of-truth bug.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_add as _impl } from '../../../src/ops/add.ts';

export function mpfr_add(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(a, b, prec, rnd);
}
