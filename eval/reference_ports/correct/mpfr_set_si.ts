/**
 * reference_ports/correct/mpfr_set_si.ts — re-export of the production port.
 *
 * See src/ops/set_si.ts for the algorithm. This wrapper exists so the
 * harness runs identical commands against `correct/<fn>.ts` and
 * `broken/<fn>.ts` by changing only the directory prefix.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_set_si as _impl } from '../../../src/ops/set_si.ts';

export function mpfr_set_si(n: bigint, prec: bigint, rnd: RoundingMode): Result {
  return _impl(n, prec, rnd);
}
