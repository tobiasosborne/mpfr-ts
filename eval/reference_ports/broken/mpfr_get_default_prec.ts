/**
 * reference_ports/broken/mpfr_get_default_prec.ts — deliberately-buggy.
 *
 * Multi-bug: (1) ignores the set call and returns 53n always,
 * (2) when the set value differs from 53, returns 53+1=54 instead.
 *
 * NOT used in production.
 */

export function mpfr_get_default_prec(prev_set: bigint): bigint {
  if (prev_set === 53n) return 53n;
  return 54n;  // BUG
}
