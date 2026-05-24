/**
 * reference_ports/broken/mpfr_lessequal_p.ts — deliberately-buggy mpfr_lessequal_p.
 *
 * **Deliberately broken: drops the equality case — returns `c < 0`
 * instead of `c <= 0`.** A plausible agent error: copy-pasted the
 * less_p body and forgot to relax the comparator. The bug shape is
 * scoped to the equality fork:
 *
 *   - a < b   : correct returns true,  broken returns true   (PASS)
 *   - a == b  : correct returns true,  broken returns false  (FAIL)
 *   - a > b   : correct returns false, broken returns false  (PASS)
 *   - NaN     : correct returns false, broken returns false  (PASS)
 *
 * So every `a == b` case is misgraded — including the same-value-
 * different-prec edge/adversarial sweep, the +0/-0 case (which equals
 * under MPFR cmp), and the same-Inf-same-sign cases. The composite
 * drops because the equality cases dominate the edge+adversarial mass
 * and the +0/-0/Inf cases are present in mined.
 *
 * Per-tag impact (assuming golden contains ~50% equal pairs across
 * edge+mined, somewhat lower in happy+fuzz):
 *
 *   corr  pass-rate ≈ 0.65 (most happy/fuzz; equality cases fail)
 *   edge  pass-rate ≈ 0.40 (lots of equal-pair edges)
 *   mined pass-rate ≈ 0.40 (equal/Inf-equal cases fail)
 *
 *   composite ≈ 0.6*0.65 + 0.2*0.40 + 0.2*0.40 ≈ 0.55
 *
 * That's slightly above the 0.5 mutation gate. The golden balances the
 * tag mass so equality-fail cases push composite below 0.5 — see
 * golden_driver.c's tag distribution.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/lessequal_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';

export function mpfr_lessequal_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  // BUG: should be `c <= 0`. Use strict `c < 0` instead — drops equality.
  return c === null ? false : c < 0;
}
