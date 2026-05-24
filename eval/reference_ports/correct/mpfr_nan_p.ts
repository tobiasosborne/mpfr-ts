/**
 * reference_ports/correct/mpfr_nan_p.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/nan_p.ts`. This wrapper exists so the harness can run
 * identical commands against `correct/<fn>.ts` and `broken/<fn>.ts`
 * by changing only the directory prefix.
 *
 * Do NOT duplicate the implementation here.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_nan_p as _impl } from '../../../src/ops/nan_p.ts';

export function mpfr_nan_p(x: MPFR): boolean {
  return _impl(x);
}
