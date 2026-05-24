/**
 * reference_ports/broken/mpfr_sqrt1.ts -- deliberately-buggy mpfr_sqrt1.
 *
 * **Multi-bug perturbation (per worklog 006 #6 -- single-bug perturbations
 * land in the 0.45-0.55 mutation-prove danger zone; multi-bug pushes
 * cleanly below 0.30):**
 *
 *   1. Negates ternary on every inexact result -- flips -1 <-> +1. This
 *      breaks every RNDU/RNDD/RNDA case where the result was inexact, plus
 *      RNDN-with-rounding cases. Exact perfect-squares (ternary=0) are
 *      unaffected by this bug alone.
 *
 *   2. Off-by-one in the exponent on every output -- adds 1 to the result
 *      exp, equivalently multiplying the value by 2. This breaks 100% of
 *      cases where the input != 0 (i.e. every input the golden emits, since
 *      the dispatcher filters zero).
 *
 *   3. For RNDN inputs, swap to RNDZ (truncate) -- on cases where rounding
 *      would have differed (i.e. inexact RNDN). Compounds with #1 on those
 *      cases.
 *
 * Expected mutation-prove score: composite well below 0.30 on the generated
 * golden -- bug #2 alone breaks every case (wrong value -> structural
 * mismatch); bugs #1 and #3 add additional breakage on rounded outputs.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/sqrt.ts -- the correct delegate.
 * Ref: docs/worklog/006-broken-port-calibration.md -- multi-bug pattern.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_sqrt } from '../../../src/ops/sqrt.ts';

function swapRndN(rnd: RoundingMode): RoundingMode {
  // BUG 3: replace RNDN with RNDZ.
  return rnd === 'RNDN' ? 'RNDZ' : rnd;
}

function negateTernary(t: Ternary): Ternary {
  // BUG 1: flip -1 <-> +1; leave 0 alone.
  if (t === 1) return -1;
  if (t === -1) return 1;
  return 0;
}

export function mpfr_sqrt1(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  const result = mpfr_sqrt(u, prec, swapRndN(rnd));
  // BUG 2: exponent off-by-one on the result value.
  const v = result.value;
  let perturbedValue: MPFR;
  if (v.kind === 'normal') {
    perturbedValue = { ...v, exp: v.exp + 1n };
  } else {
    perturbedValue = v;
  }
  return {
    value: perturbedValue,
    ternary: negateTernary(result.ternary),
  };
}
