/**
 * reference_ports/correct/mpfr_add_d.ts — re-export of the production port.
 *
 * Imports schema types for Law 4 compliance, delegates to src/ops/add_d.ts.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_add_d as _impl } from '../../../src/ops/add_d.ts';

export function mpfr_add_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(b, c, prec, rnd);
}
