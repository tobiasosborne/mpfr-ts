/**
 * reference_ports/correct/mpfr_underflow.ts — re-export of the production port.
 *
 * Mirror of correct/mpfr_overflow.ts: imports schema types for Law 4
 * compliance with the harness's AST gate, delegates to the production
 * port. Do NOT duplicate.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: CLAUDE.md PIL.3.
 */

import type { Result, RoundingMode, Sign } from '../../../src/core.ts';
import { mpfr_underflow as _impl } from '../../../src/ops/underflow.ts';

export function mpfr_underflow(
  prec: bigint,
  rnd: RoundingMode,
  sign: Sign,
): Result {
  return _impl(prec, rnd, sign);
}
