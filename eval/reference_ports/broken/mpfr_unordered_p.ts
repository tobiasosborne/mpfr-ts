/**
 * reference_ports/broken/mpfr_unordered_p.ts — deliberately-buggy mpfr_unordered_p.
 *
 * **Deliberately broken: AND instead of OR.** Returns true only when
 * BOTH operands are NaN; correct: true when AT LEAST ONE is NaN.
 *
 * Disagreement: exactly-one-NaN cases (correct: true, broken: false).
 *
 * NOT used in production. Do NOT fix.
 *
 * Ref: src/ops/unordered_p.ts.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_unordered_p(a: MPFR, b: MPFR): boolean {
  // BUG: AND instead of OR.
  return a.kind === 'nan' && b.kind === 'nan';
}
