/**
 * reference_ports/correct/mpfr_mul.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/mul.ts`. This wrapper exists so the harness can run identical
 * commands against `correct/<fn>.ts` and `broken/<fn>.ts` by changing
 * only the directory prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence between this
 * file and `src/ops/mul.ts` is a source-of-truth bug.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul as _impl } from '../../../src/ops/mul.ts';

export function mpfr_mul(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(a, b, prec, rnd);
}
