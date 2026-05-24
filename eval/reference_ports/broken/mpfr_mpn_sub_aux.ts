/**
 * reference_ports/broken/mpfr_mpn_sub_aux.ts — deliberately-buggy.
 *
 * Multi-bug: (1) ignores extra (always treats as 0), (2) ignores cy
 * (always treats as 0).
 */

export function mpfr_mpn_sub_aux(
  ap: bigint[], bp: bigint[], _cy: bigint, _extra: number,
): { result: bigint[]; borrow: bigint } {
  // BUG: ignore cy and extra; just compute ap - bp limb by limb.
  const MASK = (1n << 64n) - 1n;
  const result: bigint[] = [];
  let borrow = 0n;
  for (let i = 0; i < ap.length; i++) {
    const a = ap[i]!;
    const b = bp[i]!;
    let r = a - b - borrow;
    if (r < 0n) {
      r += 1n << 64n;
      borrow = 1n;
    } else {
      borrow = 0n;
    }
    result.push(r & MASK);
  }
  return { result, borrow };
}
