/**
 * reference_ports/correct/mpfr_setsign.ts — re-export of the production port.
 *
 * Symmetric to correct/mpfr_neg.ts / correct/mpfr_abs.ts.
 *
 * Ref: src/ops/setsign.ts — the algorithm.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_setsign as _impl } from '../../../src/ops/setsign.ts';

export function mpfr_setsign(
  x: MPFR,
  sign: boolean,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return _impl(x, sign, prec, rnd);
}
