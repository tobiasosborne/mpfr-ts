/**
 * reference_ports/correct/mpn_divrem.ts -- mutation-prove reference
 * for mpn_divrem.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline. The production src/internal/mpn/divrem.ts does
 * not yet exist; the orchestrator will materialise it during the
 * port-and-grade flow. Substrate-class -- raw bigint arrays, no
 * src/core.ts schema touch (CLAUDE.md Law 3).
 *
 * Algorithm
 * ---------
 *
 * Mirrors the mpfr-mini-gmp shim semantics (mpz_tdiv_qr):
 *
 *   1. Validate qn == 0 (MPFR_ASSERTN(qn == 0) in the shim).
 *   2. Combine np[0..nn) and dp[0..dn) into BigInt N and D.
 *   3. Q_big = N / D ; R_big = N % D.
 *   4. Decompose Q_big into nn-dn+1 little-endian limbs; the low nn-dn
 *      limbs go into q[], the (optional) (nn-dn)+1-th limb is qHigh
 *      (0n or 1n in practice).
 *   5. Decompose R_big into dn little-endian limbs (zero-padded if shorter).
 *
 * Why qHigh is at most 1: in a normalized divisor regime, the quotient
 * of an nn-limb numerator by a dn-limb divisor has at most nn - dn + 1
 * limbs, with the top limb either 0 or 1 (since the numerator is less
 * than 2 * divisor * 2^(64*(nn-dn))). For non-normalized divisors qHigh
 * could be larger; the golden enforces normalization so qHigh is 0n or 1n.
 *
 * Edge cases:
 *   - nn == dn: quotient has at most 1 limb (qHigh slot), q has length 0.
 *   - qn != 0: throw (mirrors MPFR_ASSERTN).
 *   - dn == 0: not exercised by the golden; invalid input domain.
 *
 * Ref: GMP section 8.3 -- mpn_divrem.
 * Ref: mpfr/src/mpfr-mini-gmp.c L216-L243 -- shim semantics.
 * Ref: eval/functions/mpn_divrem/spec.json -- contract.
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnDivremResult {
  readonly q: readonly bigint[];
  readonly qHigh: bigint;
  readonly r: readonly bigint[];
}

function limbsToBigInt(limbs: readonly bigint[], n: number): bigint {
  let v = 0n;
  for (let i = n - 1; i >= 0; i--) {
    const limb = limbs[i];
    if (limb === undefined) {
      throw new Error(`mpn_divrem: undefined limb at index ${i}`);
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

export function mpn_divrem(
  qn: number,
  np: readonly bigint[],
  nn: number,
  dp: readonly bigint[],
  dn: number,
): MpnDivremResult {
  // Step 1: validate qn == 0 (MPFR_ASSERTN(qn == 0) per the shim).
  if (qn !== 0) {
    throw new Error(`mpn_divrem: qn must be 0 (MPFR shim asserts), got ${qn}`);
  }
  if (!Number.isInteger(nn) || nn < 0) {
    throw new Error(`mpn_divrem: nn must be non-negative integer, got ${nn}`);
  }
  if (!Number.isInteger(dn) || dn < 1) {
    throw new Error(`mpn_divrem: dn must be >= 1, got ${dn}`);
  }
  if (nn < dn) {
    throw new Error(`mpn_divrem: nn must be >= dn, got nn=${nn}, dn=${dn}`);
  }
  if (np.length < nn) {
    throw new Error(`mpn_divrem: np too short: need ${nn}, got ${np.length}`);
  }
  if (dp.length < dn) {
    throw new Error(`mpn_divrem: dp too short: need ${dn}, got ${dp.length}`);
  }

  // Step 2: combine to BigInt.
  const N = limbsToBigInt(np, nn);
  const D = limbsToBigInt(dp, dn);
  if (D === 0n) {
    throw new Error(`mpn_divrem: divisor is zero`);
  }

  // Step 3: divide.
  const Q = N / D;
  const R = N % D;

  // Step 4: decompose quotient. Total quotient limbs needed = nn - dn + 1
  // (with the top slot being qHigh).
  const totalQLimbs = nn - dn + 1;
  const qAll = bigIntToLimbs(Q, totalQLimbs);
  // q[] is the low nn - dn limbs; qHigh is the (nn-dn)+1-th limb.
  const q = qAll.slice(0, nn - dn);
  const qHighOpt = qAll[nn - dn];
  const qHigh: bigint = qHighOpt === undefined ? 0n : qHighOpt;

  // Step 5: decompose remainder into dn little-endian limbs.
  const r = bigIntToLimbs(R, dn);

  return { q: q as readonly bigint[], qHigh, r: r as readonly bigint[] };
}
