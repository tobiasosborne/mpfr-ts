/**
 * reference_ports/correct/mpfr_trunc.ts — re-export of the production port.
 *
 * Ref: src/ops/trunc.ts — the algorithm.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_trunc as _impl } from '../../../src/ops/trunc.ts';

export function mpfr_trunc(x: MPFR, prec: bigint): Result {
  return _impl(x, prec);
}
