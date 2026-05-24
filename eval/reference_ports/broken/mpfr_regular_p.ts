/**
 * reference_ports/broken/mpfr_regular_p.ts — deliberately-buggy mpfr_regular_p.
 *
 * Polarity flip: returns `kind !== 'normal'` instead of `kind === 'normal'`.
 * Every case flips → composite ~ 0. (Polarity-flip is acceptable here
 * because the predicate-family golden has well-balanced true/false cases;
 * no need for multi-bug.)
 *
 * NOT used in production. Do NOT fix.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_regular_p(x: MPFR): boolean {
  return x.kind !== 'normal';
}
