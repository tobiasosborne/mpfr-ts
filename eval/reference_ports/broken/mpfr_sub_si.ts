/**
 * reference_ports/broken/mpfr_sub_si.ts — deliberately-buggy mpfr_sub_si.
 *
 * **Multi-bug perturbation:**
 *   1. Dispatch INVERTED: c >= 0 routes to add_ui (should be sub_ui),
 *      c < 0 routes to sub_ui (should be add_ui). Every c != 0 case
 *      gets the wrong sign of subtraction.
 *
 * NOT used in production.
 *
 * Ref: src/ops/sub_si.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { mpfr_add_ui } from '../../../src/ops/add_ui.ts';
import { mpfr_sub_ui } from '../../../src/ops/sub_ui.ts';

const LONG_MIN_VAL: bigint = -(1n << 63n);
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

export function mpfr_sub_si(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  if (typeof c !== 'bigint') throw new MPFRError('EPREC', `c bigint`);
  if (c < LONG_MIN_VAL || c > LONG_MAX_VAL) throw new MPFRError('EPREC', `c range`);
  // BUG: dispatch inverted.
  if (c >= 0n) {
    return mpfr_add_ui(b, c, prec, rnd);
  }
  return mpfr_sub_ui(b, -c, prec, rnd);
}
