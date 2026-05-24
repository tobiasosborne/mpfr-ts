/**
 * reference_ports/broken/mpfr_lessgreater_p.ts — deliberately-buggy.
 *
 * **Deliberately broken: returns `compareMPFR(a, b) === 0` (inverted).**
 *
 *   - Equal pair: correct false, broken true   (FAIL)
 *   - Strictly less / greater: correct true, broken false  (FAIL)
 *   - NaN: correct false; compareMPFR returns null; `null === 0` is
 *          false → broken false  (PASS — coincidentally agrees)
 *
 * Fails on most non-NaN cases. NaN cases coincidentally pass; the
 * driver compensates with a heavy mix of equal and strict-ordered
 * pairs in adversarial + fuzz.
 *
 * NOT used in production. Do NOT fix.
 *
 * Ref: src/ops/lessgreater_p.ts.
 */

import type { MPFR } from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';

export function mpfr_lessgreater_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  // BUG: invert — returns true on equal, false on ordered.
  return c === 0;
}
