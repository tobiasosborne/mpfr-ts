/**
 * reference_ports/broken/mpfr_mul_ui5.ts -- deliberately-buggy.
 *
 * **Collapses the entire decision tree to a constant output** (per
 * HANDOFF gotcha #10): every input returns x itself unchanged (no
 * multiplications applied, ternary=0). Every non-identity case fails
 * on strict equality; composite well below 0.30.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError, PREC_MAX, PREC_MIN,
} from '../../../src/core.ts';
import { mpfr_set } from '../../../src/ops/set.ts';

const ULONG_MAX = (1n << 64n) - 1n;

export function mpfr_mul_ui5(
  x: MPFR,
  v1: bigint, v2: bigint, v3: bigint, v4: bigint, v5: bigint,
  prec: bigint, rnd: RoundingMode,
): Result {
  for (const [name, v] of [
    ['v1', v1], ['v2', v2], ['v3', v3], ['v4', v4], ['v5', v5],
  ] as Array<[string, bigint]>) {
    if (typeof v !== 'bigint' || v <= 0n || v > ULONG_MAX) {
      throw new MPFRError('EDOMAIN', `mpfr_mul_ui5: bad ${name}`);
    }
  }
  if (typeof prec !== 'bigint' || prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_mul_ui5: bad prec`);
  }
  if (rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA') {
    throw new MPFRError('EROUND', `mpfr_mul_ui5: unknown rnd`);
  }
  // BUG: ignore v_i entirely; return x unchanged.
  return mpfr_set(x, prec, rnd);
}
