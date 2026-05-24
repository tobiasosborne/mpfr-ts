/**
 * reference_ports/correct/mpfr_cmp_si.ts — re-export of the production port.
 *
 * See src/ops/cmp_si.ts for the algorithm.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp_si as _impl } from '../../../src/ops/cmp_si.ts';

export function mpfr_cmp_si(x: MPFR, n: bigint): number {
  return _impl(x, n);
}
