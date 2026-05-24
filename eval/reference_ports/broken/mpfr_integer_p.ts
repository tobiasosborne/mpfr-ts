/**
 * reference_ports/broken/mpfr_integer_p.ts — deliberately-buggy mpfr_integer_p.
 *
 * Multi-bug: (1) inverts the answer for normal values (true integers
 * report false, fractions report true), (2) reports zero as non-integer,
 * (3) reports NaN/Inf as integers.
 *
 * NOT used in production.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_integer_p(x: MPFR): boolean {
  if (x.kind === 'zero') return false;       // BUG: zero IS an integer
  if (x.kind === 'nan' || x.kind === 'inf') return true;  // BUG: not integers
  // Compute the correct integer-check, then INVERT the answer.
  const intBits = x.exp > 0n ? x.exp : 0n;
  if (intBits >= x.prec) return false;       // BUG: pure integers -> false
  const fracBits = x.prec - intBits;
  const fracMask = (1n << fracBits) - 1n;
  const fracIsZero = (x.mant & fracMask) === 0n;
  return !fracIsZero;                         // BUG: inverted
}
