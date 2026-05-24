/**
 * reference_ports/broken/mpfr_odd_p.ts — deliberately-buggy mpfr_odd_p.
 *
 * Multi-bug: (1) inverts the parity bit (even-as-odd, odd-as-even),
 * (2) returns true for inf inputs, (3) skips the fraction-bits-zero
 * guard so 1.5 reports as the parity of 1.
 *
 * NOT used in production.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_odd_p(x: MPFR): boolean {
  if (x.kind === 'inf') return true;        // BUG: inf is not odd
  if (x.kind !== 'normal') return false;
  if (x.exp <= 0n) return false;
  if (x.exp > x.prec) return false;
  // Locate the unit bit; correct impl also requires fraction bits == 0.
  const unitBitPos = x.prec - x.exp;
  // BUG: skip the fraction-bits check entirely.
  // BUG: invert the parity (return 0 when bit set, 1 when clear).
  return ((x.mant >> unitBitPos) & 1n) === 0n;
}
