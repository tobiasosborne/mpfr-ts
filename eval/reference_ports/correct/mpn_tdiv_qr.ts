/**
 * reference_ports/correct/mpn_tdiv_qr.ts -- mutation-prove reference
 * for mpn_tdiv_qr.
 *
 * Substrate-class -- raw bigint arrays, no src/core.ts schema touch
 * (CLAUDE.md Law 3).
 *
 * Algorithm
 * ---------
 *
 * Mirrors the mpfr-mini-gmp shim semantics (mpfr/src/mpfr-mini-gmp.c
 * L246-L262):
 *
 *   1. Validate qxn == 0 (MPFR_ASSERTN(qxn == 0) in the shim).
 *   2. Combine np[0..nn) and dp[0..dn) into BigInt N and D.
 *   3. Q_big = N / D ; R_big = N % D (bigint truncated division).
 *   4. Decompose Q_big into exactly nn-dn+1 little-endian limbs (top
 *      limb is mpn_divrem's qHigh analog).
 *   5. Decompose R_big into exactly dn little-endian limbs.
 *
 * Delegates to the shipped src/internal/mpn/divrem.ts -- mpn_divrem
 * returns {q, qHigh, r}; the tdiv_qr shape is {q', r} where q' is
 * [...q, qHigh] (length nn-dn+1).
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnTdivQrResult {
  readonly q: readonly bigint[];   // length nn - dn + 1
  readonly r: readonly bigint[];   // length dn
}

function limbsToBigInt(limbs: readonly bigint[], n: number): bigint {
  let v = 0n;
  for (let i = n - 1; i >= 0; i--) {
    const limb = limbs[i];
    if (limb === undefined) {
      throw new Error(`mpn_tdiv_qr: undefined limb at index ${i}`);
    }
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
  if (qxn !== 0) {
    throw new Error(`mpn_tdiv_qr: qxn must be 0 (MPFR shim asserts), got ${qxn}`);
  }
  if (nn < dn) {
    throw new Error(`mpn_tdiv_qr: requires nn >= dn, got nn=${nn} dn=${dn}`);
  }
  if (dn < 1) {
    throw new Error(`mpn_tdiv_qr: requires dn >= 1, got dn=${dn}`);
  }

  const N = limbsToBigInt(np, nn);
  const D = limbsToBigInt(dp, dn);
  if (D === 0n) {
    throw new Error(`mpn_tdiv_qr: divisor is zero`);
  }
  const Q = N / D;  // bigint truncated division (toward zero); both positive here.
  const R = N - Q * D;

  const q = bigIntToLimbs(Q, nn - dn + 1);
  const r = bigIntToLimbs(R, dn);

  return { q, r };
}
