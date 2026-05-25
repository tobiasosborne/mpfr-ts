/**
 * reference_ports/broken/mpn_divrem.ts -- deliberately-buggy mpn_divrem.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpn_divrem golden, the golden is too weak.
 *
 * **Deliberately broken: swap quotient and remainder.** Returns the
 * remainder under the `q` key and the (truncated) quotient under the
 * `r` key. Every output triple has q and r swapped, so the comparison
 * against the golden's true {q, r} pair fails on every case where
 * Q != R (which is essentially all of them -- only the degenerate
 * "numerator is zero" cases produce q = r = 0 and accidentally pass).
 *
 * Why this bug shape: the variable names `q` and `r` are easy to swap
 * in a refactor, especially in the mini-gmp shim code where the
 * mpz_tdiv_qr call writes both quotient and remainder. An agent who
 * reads `mpz_tdiv_qr(q, r, n, d)` and assumes the first arg is
 * remainder (which is correct for some other mpz_*div_* variants)
 * would write the swap.
 *
 * Expected failure surface:
 *   - happy: 20/20 fail (every fuzz-generated case has Q != R).
 *   - edge: 30/31 fail (the "np = 0" case passes; q = r = 0).
 *   - adversarial: 12/12 fail.
 *   - fuzz: 60/60 fail.
 * Total: ~122 of 123 fail -- composite ~0.008. Well outside danger zone.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 -- mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 -- composite must drop below 0.55 under mutation.
 * Ref: eval/reference_ports/correct/mpn_divrem.ts -- correct version.
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
  if (qn !== 0) {
    throw new Error(`mpn_divrem: qn must be 0, got ${qn}`);
  }
  if (!Number.isInteger(nn) || nn < 0) {
    throw new Error(`mpn_divrem: nn invalid`);
  }
  if (!Number.isInteger(dn) || dn < 1) {
    throw new Error(`mpn_divrem: dn invalid`);
  }
  if (nn < dn) {
    throw new Error(`mpn_divrem: nn < dn`);
  }
  if (np.length < nn || dp.length < dn) {
    throw new Error(`mpn_divrem: operands too short`);
  }

  const N = limbsToBigInt(np, nn);
  const D = limbsToBigInt(dp, dn);
  if (D === 0n) {
    throw new Error(`mpn_divrem: divisor zero`);
  }

  // BUG: swap Q and R. Should be `const Q = N / D; const R = N % D`.
  const Q = N % D;
  const R = N / D;

  const totalQLimbs = nn - dn + 1;
  const qAll = bigIntToLimbs(Q, totalQLimbs);
  const q = qAll.slice(0, nn - dn);
  const qHighOpt = qAll[nn - dn];
  const qHigh: bigint = qHighOpt === undefined ? 0n : qHighOpt;
  const r = bigIntToLimbs(R, dn);

  return { q: q as readonly bigint[], qHigh, r: r as readonly bigint[] };
}
