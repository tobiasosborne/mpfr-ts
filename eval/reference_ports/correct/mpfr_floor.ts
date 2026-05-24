/**
 * reference_ports/correct/mpfr_floor.ts — re-export of the production port.
 *
 * Ref: src/ops/floor.ts — the algorithm.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_floor as _impl } from '../../../src/ops/floor.ts';

export function mpfr_floor(x: MPFR, prec: bigint): Result {
  return _impl(x, prec);
}
