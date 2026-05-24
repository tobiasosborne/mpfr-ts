/**
 * reference_ports/correct/mpfr_get_prec.ts — re-export of the production
 * port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout, the "correct"
 * reference IS the production implementation — `src/ops/get_prec.ts`.
 * This wrapper exists so the harness can run identical commands against
 * `correct/<fn>.ts` and `broken/<fn>.ts` by changing only the directory
 * prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence is a
 * source-of-truth bug: the production op IS the reference.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_get_prec as _impl } from '../../../src/ops/get_prec.ts';

export function mpfr_get_prec(x: MPFR): bigint {
  return _impl(x);
}
