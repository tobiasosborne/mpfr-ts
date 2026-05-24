/**
 * reference_ports/correct/mpfr_ceil.ts — re-export of the production port.
 *
 * Ref: src/ops/ceil.ts — the algorithm.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_ceil as _impl } from '../../../src/ops/ceil.ts';

export function mpfr_ceil(x: MPFR, prec: bigint): Result {
  return _impl(x, prec);
}
