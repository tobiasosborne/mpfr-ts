/**
 * reference_ports/broken/mpfr_div2_approx.ts — deliberately-buggy.
 *
 * Multi-bug: (1) returns the high 128 bits of (u >> v) instead of u*B^2/v,
 * (2) swaps Q1 and Q0.
 */

export function mpfr_div2_approx(
  u1: bigint, u0: bigint, v1: bigint, v0: bigint,
): { Q1: bigint; Q0: bigint } {
  const MASK = (1n << 64n) - 1n;
  const u = (u1 << 64n) | u0;
  const v = (v1 << 64n) | v0;
  // BUG: returns u/v (no shift), then swaps Q1/Q0.
  const q = v === 0n ? 0n : u / v;
  return {
    Q1: q & MASK,            // BUG: swapped (should be high half)
    Q0: (q >> 64n) & MASK,   // BUG: swapped (should be low half)
  };
}
