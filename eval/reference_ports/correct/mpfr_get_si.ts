/**
 * reference_ports/correct/mpfr_get_si.ts — re-export of the production port.
 *
 * See src/ops/get_si.ts for the algorithm.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, RoundingMode } from '../../../src/core.ts';
import { mpfr_get_si as _impl } from '../../../src/ops/get_si.ts';

export function mpfr_get_si(x: MPFR, rnd: RoundingMode): bigint {
  return _impl(x, rnd);
}
