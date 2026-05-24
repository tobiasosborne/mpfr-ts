/**
 * reference_ports/broken/mpfr_mpn_cmp_aux.ts — deliberately-buggy.
 *
 * Multi-bug: (1) ignores extra (always treats as 0), (2) returns
 * sign of (b - a) instead of (a - b).
 */

export function mpfr_mpn_cmp_aux(ap: bigint[], bp: bigint[], _extra: number): number {
  // Reassemble little-endian limb arrays into bigints, then compare.
  let a = 0n, b = 0n;
  for (let i = ap.length - 1; i >= 0; i--) a = (a << 64n) | ap[i]!;
  for (let i = bp.length - 1; i >= 0; i--) b = (b << 64n) | bp[i]!;
  // BUG: ignore extra; negate result.
  if (a > b) return -1;  // BUG: negated
  if (a < b) return 1;   // BUG: negated
  return 0;
}
