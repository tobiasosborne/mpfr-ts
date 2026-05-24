/**
 * reference_ports/correct/mpfr_set_ui.ts — re-export of the production port.
 *
 * See src/ops/set_ui.ts for the algorithm.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_set_ui as _impl } from '../../../src/ops/set_ui.ts';

export function mpfr_set_ui(n: bigint, prec: bigint, rnd: RoundingMode): Result {
  return _impl(n, prec, rnd);
}
