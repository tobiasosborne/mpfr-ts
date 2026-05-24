/**
 * reference_ports/broken/mpfr_add_si.ts — deliberately-buggy mpfr_add_si.
 *
 * **Multi-bug perturbation:**
 *   1. The c >= 0 / c < 0 dispatch is INVERTED: positive c routes to
 *      sub_ui, negative c routes to add_ui. Every case with c != 0 fails.
 *   2. For c == 0: doesn't short-circuit (relies on add_ui with c=0,
 *      but routes wrong way — actually OK here since both add_ui(0)
 *      and sub_ui(0) return mpfr_set, so c=0 cases pass).
 *
 * NOT used in production.
 *
 * Ref: src/ops/add_si.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { mpfr_add_ui } from '../../../src/ops/add_ui.ts';
import { mpfr_sub_ui } from '../../../src/ops/sub_ui.ts';

const LONG_MIN_VAL: bigint = -(1n << 63n);
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

export function mpfr_add_si(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  if (typeof c !== 'bigint') throw new MPFRError('EPREC', `c bigint`);
  if (c < LONG_MIN_VAL || c > LONG_MAX_VAL) throw new MPFRError('EPREC', `c range`);
  // BUG 1: dispatch inverted.
  if (c >= 0n) {
    return mpfr_sub_ui(b, c, prec, rnd);
  }
  return mpfr_add_ui(b, -c, prec, rnd);
}
