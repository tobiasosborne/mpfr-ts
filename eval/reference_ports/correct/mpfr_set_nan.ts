/**
 * reference_ports/correct/mpfr_set_nan.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout (docs/PILOT_PLAN.md
 * Step 7), the "correct" reference IS the production implementation —
 * `src/ops/set_nan.ts`. This wrapper exists so the harness can run
 * identical commands against both reference ports.
 *
 * Do NOT duplicate the implementation here.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_set_nan as _impl } from '../../../src/ops/set_nan.ts';

export function mpfr_set_nan(): MPFR {
  return _impl();
}
