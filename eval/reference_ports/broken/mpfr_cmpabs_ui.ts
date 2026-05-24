/**
 * reference_ports/broken/mpfr_cmpabs_ui.ts — deliberately-buggy mpfr_cmpabs_ui.
 *
 * Multi-bug: (1) compares b directly (signed) instead of |b|, (2) negates
 * the comparison result, (3) returns 0 for any normal b whose mantissa
 * has the top two bits both set (a wrong "shortcut").
 *
 * NOT used in production.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_cmp_ui } from '../../../src/ops/cmp_ui.ts';

export function mpfr_cmpabs_ui(b: MPFR, c: bigint): number {
  if (b.kind === 'normal') {
    // BUG: bogus shortcut on top-two-bits-set.
    const topTwo = b.mant >> (b.prec - 2n);
    if (topTwo === 3n) return 0;
  }
  // BUG 1: pass b directly (sign included). BUG 2: negate result.
  return -mpfr_cmp_ui(b, c);
}
