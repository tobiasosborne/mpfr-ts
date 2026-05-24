/**
 * reference_ports/correct/mpfr_overflow.ts — re-export of the production port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout, the "correct"
 * reference IS the production implementation under `src/ops/overflow.ts`.
 * The thin wrapper here imports types from the locked schema (Law 4
 * compliance for the harness's AST gate) and delegates to the
 * production port. Do NOT duplicate the implementation; the production
 * op IS the reference.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: CLAUDE.md PIL.3.
 */

import type { Result, RoundingMode, Sign } from '../../../src/core.ts';
import { mpfr_overflow as _impl } from '../../../src/ops/overflow.ts';

export function mpfr_overflow(
  prec: bigint,
  rnd: RoundingMode,
  sign: Sign,
): Result {
  return _impl(prec, rnd, sign);
}
