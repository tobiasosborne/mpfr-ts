/**
 * reference_ports/broken/mpfr_greater_p.ts — deliberately-buggy mpfr_greater_p.
 *
 * **Deliberately broken: returns `!correctAnswer`.** Every case answer
 * is flipped. See `./mpfr_less_p.ts` for the rationale; the same
 * mutation pattern applies here. Every case fails → composite → 0.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/greater_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';

export function mpfr_greater_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  // BUG: NEGATE the correct answer.
  return !(c === null ? false : c > 0);
}
