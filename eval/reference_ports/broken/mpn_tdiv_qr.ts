/**
 * reference_ports/broken/mpn_tdiv_qr.ts -- deliberately-buggy.
 *
 * **BUG: swaps the quotient and remainder.** Returns {q: R-limbs,
 * r: Q-limbs-truncated}. Sizes won't match for most cases either,
 * triggering shape mismatches. Every case fails.
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnTdivQrResult {
  readonly q: readonly bigint[];
  readonly r: readonly bigint[];
}

function limbsToBigInt(limbs: readonly bigint[], n: number): bigint {
  let v = 0n;
  for (let i = n - 1; i >= 0; i--) {
    const limb = limbs[i];
    if (limb === undefined) throw new Error(`bad limb`);
    v = (v << LIMB_BITS) | limb;
  }
  return v;
}

function bigIntToLimbs(v: bigint, n: number): bigint[] {
  const out: bigint[] = new Array<bigint>(n);
  let remaining = v;
  for (let i = 0; i < n; i++) {
    out[i] = remaining & LIMB_MASK;
    remaining >>= LIMB_BITS;
  }
  return out;
}

export function mpn_tdiv_qr(
  qxn: number,
  np: readonly bigint[],
  nn: number,
  dp: readonly bigint[],
  dn: number,
): MpnTdivQrResult {
  if (qxn !== 0) throw new Error(`mpn_tdiv_qr: bad qxn`);
  if (nn < dn || dn < 1) throw new Error(`mpn_tdiv_qr: bad sizes`);

  const N = limbsToBigInt(np, nn);
  const D = limbsToBigInt(dp, dn);
  if (D === 0n) throw new Error(`mpn_tdiv_qr: divisor zero`);
  const Q = N / D;
  const R = N - Q * D;

  // BUG: swap q and r outputs.
  return {
    q: bigIntToLimbs(R, nn - dn + 1),
    r: bigIntToLimbs(Q, dn),
  };
}
