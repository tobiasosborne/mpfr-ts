/**
 * reference_ports/correct/mpfr_mul_2si.ts — re-export of the production port.
 *
 * See src/ops/mul_2si.ts for the algorithm.
 *
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_mul_2si as _impl } from '../../../src/ops/mul_2si.ts';

export function mpfr_mul_2si(
  x: MPFR,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(x, e, prec, rnd);
}
