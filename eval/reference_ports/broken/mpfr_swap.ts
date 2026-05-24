/**
 * reference_ports/broken/mpfr_swap.ts — deliberately-buggy mpfr_swap.
 *
 * **Multi-bug perturbation:**
 *   1. Returns { a, b } (no swap) instead of { a: b, b: a }. Every
 *      case where a and b differ structurally fails on at least one
 *      field.
 *   2. Flips the sign of the a-field for normal kind so same-pair
 *      cases (a == b) also break.
 *
 * NOT used in production.
 *
 * Ref: src/ops/swap.ts — the correct version.
 */

import type { MPFR, Sign } from '../../../src/core.ts';

export function mpfr_swap(
  a: MPFR,
  b: MPFR,
): { readonly a: MPFR; readonly b: MPFR } {
  const first: MPFR =
    a.kind === 'normal'
      ? { ...a, sign: (-a.sign) as Sign }
      : a;
  // BUG 1: no swap (return original order).
  return { a: first, b: b };
}
