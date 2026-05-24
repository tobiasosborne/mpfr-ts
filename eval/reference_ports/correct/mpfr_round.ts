/**
 * reference_ports/correct/mpfr_round.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout, the "correct"
 * reference IS the production implementation — `src/ops/round.ts`.
 *
 * Do NOT duplicate the implementation here. The production op IS the
 * reference.
 *
 * Ref: src/ops/round.ts — the algorithm.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_round as _impl } from '../../../src/ops/round.ts';

export function mpfr_round(x: MPFR, prec: bigint): Result {
  return _impl(x, prec);
}
