/**
 * reference_ports/broken/mpfr_mpn_cmpzero.ts — deliberately-buggy.
 *
 * Multi-bug: (1) returns 0 always (the polarity is fully flipped),
 * (2) only checks the first limb (so [0, 1] returns 0 wrongly).
 */

export function mpfr_mpn_cmpzero(ap: bigint[]): number {
  // BUG: only checks ap[0]; even then returns wrong polarity.
  if (ap.length === 0) return 0;
  return ap[0] === 0n ? 1 : 0;  // BUG: flipped
}
