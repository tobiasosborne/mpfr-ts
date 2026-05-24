/**
 * reference_ports/correct/mpfr_get_ui.ts — re-export of the production port.
 *
 * See src/ops/get_ui.ts for the algorithm.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, RoundingMode } from '../../../src/core.ts';
import { mpfr_get_ui as _impl } from '../../../src/ops/get_ui.ts';

export function mpfr_get_ui(x: MPFR, rnd: RoundingMode): bigint {
  return _impl(x, rnd);
}
