/**
 * reference_ports/correct/mpfr_setmin.ts — re-export of the production
 * port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout, the "correct"
 * reference IS the production implementation — `src/ops/setmin.ts`.
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

import type { MPFR, Sign } from '../../../src/core.ts';
import { mpfr_setmin as _impl } from '../../../src/ops/setmin.ts';

export function mpfr_setmin(prec: bigint, exp: bigint, sign: Sign = 1): MPFR {
  return _impl(prec, exp, sign);
}
