/**
 * reference_ports/correct/mpfr_nexttoward.ts -- mutation-prove reference.
 *
 * Step x one ULP toward y at x's own precision (IEEE 754-1985 nextafter
 * restricted to MPFR's precision lattice). NOT IEEE 754-2008 nextUp/nextDown.
 *
 * Algorithm (mpfr/src/next.c L148-L172):
 *   - x is NaN -> NaN.
 *   - y is NaN -> NaN.
 *   - cmp(x, y) == 0 -> return x unchanged (NOT y; IEEE 754-1985).
 *   - cmp(x, y) < 0 -> nextabove(x).
 *   - cmp(x, y) > 0 -> nextbelow(x).
 *
 * The cmp == 0 case preserves x's sign of zero for +/-0 inputs of opposite
 * sign (IEEE 754-1985 nextafter behaviour, NOT ISO C99 nexttoward).
 *
 * Note: src/ops/cmp.ts throws on NaN inputs; we MUST guard NaN before
 * calling it. The C cmp returns 0 with erange flag set on NaN, but the
 * TS surface doesn't model erange (per HANDOFF), so the explicit guard
 * is load-bearing for correctness.
 *
 * Ref: mpfr/src/next.c L148-L172 -- C reference.
 * Ref: mpfr/src/next.c L23-L41 -- IEEE 1985 vs 2008 distinction.
 * Ref: src/ops/nextabove.ts, src/ops/nextbelow.ts -- delegates.
 * Ref: src/ops/cmp.ts -- ordering (throws on NaN).
 */

import type { MPFR } from '../../../src/core.ts';
import { NAN_VALUE } from '../../../src/core.ts';
import { mpfr_cmp } from '../../../src/ops/cmp.ts';
import { mpfr_nextabove } from '../../../src/ops/nextabove.ts';
import { mpfr_nextbelow } from '../../../src/ops/nextbelow.ts';

export function mpfr_nexttoward(x: MPFR, y: MPFR): MPFR {
  // NaN guards FIRST (cmp throws on NaN per the TS surface).
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }
  if (y.kind === 'nan') {
    return NAN_VALUE;
  }
  // cmp returns sign of (x - y); throws on NaN (already guarded).
  const c = mpfr_cmp(x, y);
  if (c === 0) {
    // IEEE 754-1985: return x unchanged (including +/-0 sign).
    return x;
  }
  if (c < 0) {
    return mpfr_nextabove(x);
  }
  return mpfr_nextbelow(x);
}
