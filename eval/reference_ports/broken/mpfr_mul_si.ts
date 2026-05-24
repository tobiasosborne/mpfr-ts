/**
 * reference_ports/broken/mpfr_mul_si.ts — deliberately-buggy mpfr_mul_si.
 *
 * **Multi-bug perturbation (covers both branches):**
 *   1. For c >= 0: delegates to mpfr_mul_ui(b, c+1, prec, rnd) (off-by-one
 *      in the integer operand). Every c >= 0 case fails on value.
 *   2. For c < 0: does NOT invert rnd, does NOT negate ternary, does NOT
 *      flip sign. Wrong direction, wrong sign, wrong ternary.
 *   3. For c == 0: c+1 = 1, so the c == 0 cases produce b * 1 = b
 *      instead of b * 0 = 0. Massive value mismatch.
 *
 * NOT used in production.
 *
 * Ref: src/ops/mul_si.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { mpfr_mul_ui } from '../../../src/ops/mul_ui.ts';

const LONG_MIN_VAL: bigint = -(1n << 63n);
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

export function mpfr_mul_si(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  if (typeof c !== 'bigint') throw new MPFRError('EPREC', `c bigint`);
  if (c < LONG_MIN_VAL || c > LONG_MAX_VAL) throw new MPFRError('EPREC', `c range`);
  if (c >= 0n) {
    // BUG 1: c+1 instead of c. Within LONG_MAX bound for most fuzz cases.
    const cc = c < LONG_MAX_VAL ? c + 1n : c;
    return mpfr_mul_ui(b, cc, prec, rnd);
  }
  // BUG 2: no invert rnd, no negate ternary, no flip sign.
  return mpfr_mul_ui(b, -c, prec, rnd);
}
