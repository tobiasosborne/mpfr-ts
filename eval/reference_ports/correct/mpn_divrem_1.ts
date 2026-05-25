/**
 * reference_ports/correct/mpn_divrem_1.ts -- mutation-prove reference
 * for mpn_divrem_1.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline. The production src/internal/mpn/divrem_1.ts
 * does not yet exist; the orchestrator will materialise it during the
 * port-and-grade flow. Substrate-class -- raw bigint arrays, no
 * src/core.ts schema touch (CLAUDE.md Law 3).
 *
 * Algorithm
 * ---------
 *
 * The mini-gmp shim wraps the division in mpz_tdiv_qr -- semantically
 * identical to native BigInt truncated division:
 *
 *   1. Combine np[0..nn) (little-endian) into a single dividend BigInt.
 *   2. Shift left by (qxn * 64) bits to prepend the qxn zero limbs.
 *   3. q_bigint = dividend / d0; r_bigint = dividend % d0.
 *   4. Decompose q_bigint into (nn + qxn) little-endian 64-bit limbs.
 *
 * Returns { q, r } where q has length nn + qxn (pad with zeros if the
 * BigInt quotient is smaller; e.g. nn=1, d0 > np[0] yields q=[0n], r=np[0]).
 *
 * Edge cases:
 *   - nn = 0, qxn = 0: empty dividend, returns { q: [], r: 0n }.
 *   - nn = 0, qxn > 0: dividend = 0 (qxn zero limbs); returns
 *     { q: array of qxn zeros, r: 0n }. (Note: real GMP mpn_divrem_1
 *     may not accept nn=0; the golden does not emit this case.)
 *   - d0 = 0: domain error.
 *
 * Ref: GMP section 8.3 -- mpn_divrem_1.
 * Ref: mpfr/src/mpfr-mini-gmp.c L177-L213 -- shim semantics.
 * Ref: eval/functions/mpn_divrem_1/spec.json -- contract.
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnDivrem1Result {
  readonly q: readonly bigint[];
  readonly r: bigint;
}

export function mpn_divrem_1(
  qxn: number,
  np: readonly bigint[],
  nn: number,
  d0: bigint,
): MpnDivrem1Result {
  if (!Number.isInteger(qxn) || qxn < 0) {
    throw new Error(`mpn_divrem_1: qxn must be a non-negative integer, got ${qxn}`);
  }
  if (!Number.isInteger(nn) || nn < 0) {
    throw new Error(`mpn_divrem_1: nn must be a non-negative integer, got ${nn}`);
  }
  if (np.length < nn) {
    throw new Error(`mpn_divrem_1: np too short: need ${nn} limbs, got ${np.length}`);
  }
  if (typeof d0 !== 'bigint') {
    throw new Error(`mpn_divrem_1: d0 must be bigint, got ${typeof d0}`);
  }
  if (d0 <= 0n) {
    throw new Error(`mpn_divrem_1: d0 must be > 0, got ${d0}`);
  }

  // Step 1: combine np[0..nn) into a BigInt. Little-endian: limb[0] is LSB.
  let dividend = 0n;
  for (let i = nn - 1; i >= 0; i--) {
    const limb = np[i];
    if (limb === undefined) {
      throw new Error(`mpn_divrem_1: internal invariant violated -- undefined limb at index ${i}`);
    }
    dividend = (dividend << LIMB_BITS) | limb;
  }

  // Step 2: prepend qxn zero limbs at the LSB end (shift left by qxn * 64).
  dividend <<= BigInt(qxn) * LIMB_BITS;

  // Step 3: divide.
  const qBig = dividend / d0;
  const r = dividend % d0;

  // Step 4: decompose quotient into (nn + qxn) little-endian limbs.
  const qLen = nn + qxn;
  const q: bigint[] = new Array<bigint>(qLen);
  let remaining = qBig;
  for (let i = 0; i < qLen; i++) {
    q[i] = remaining & LIMB_MASK;
    remaining >>= LIMB_BITS;
  }
  // If the BigInt quotient was larger than (nn + qxn) limbs we silently
  // drop the high bits -- but the C contract guarantees this cannot
  // happen for valid inputs (the quotient of an (nn + qxn)-limb integer
  // by a 1-limb divisor fits in nn + qxn limbs).

  return { q: q as readonly bigint[], r };
}
