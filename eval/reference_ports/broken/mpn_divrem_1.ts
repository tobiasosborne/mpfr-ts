/**
 * reference_ports/broken/mpn_divrem_1.ts -- deliberately-buggy mpn_divrem_1.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpn_divrem_1 golden, the golden is too weak.
 *
 * **Deliberately broken: qxn shift is OMITTED.** The correct port
 * shifts the dividend left by `qxn * 64` bits before dividing (to
 * prepend the qxn zero limbs at the LSB). The broken port skips this
 * shift, so every case with qxn > 0 produces a wrong quotient (it
 * computes np / d0 instead of (np * 2^(64*qxn)) / d0).
 *
 * Why this bug shape: the qxn parameter is non-obvious -- an agent who
 * reads only the function name "divrem_1" and skims the np/d0 arguments
 * could conclude qxn is just an output-buffer length hint, not part of
 * the dividend. The shim source at mpfr/src/mpfr-mini-gmp.c L189-L201
 * explicitly handles qxn != 0 by building a longer mpz_init2'd
 * numerator and copying np into the high end -- the very behavior the
 * shift simulates. Dropping it is a one-line omission.
 *
 * Expected failure surface:
 *   - happy: qxn = 0 in all cases (21/21 pass; this branch is correct).
 *   - edge: 6 of 32 cases have qxn > 0 -> fail.
 *   - adversarial: 2 of 12 cases have qxn > 0 -> fail.
 *   - fuzz: ~4/5 cases have qxn = 0 by uniform sampling [0,4], but ~12
 *     of 60 cases have qxn > 0 -> fail.
 *
 * Total fail count: ~20 of 125 -- composite ~0.84. INSIDE the
 * problematic band (>0.55). Need a stronger perturbation.
 *
 * Adopted final perturbation: ALSO swap np walk direction so the
 * dividend is reassembled in big-endian (reversed) order. This makes
 * EVERY case with nn > 1 produce a wrong dividend, dropping composite
 * to ~0.10.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 -- mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 -- composite must drop below 0.55 under mutation.
 * Ref: eval/reference_ports/correct/mpn_divrem_1.ts -- correct version.
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
    throw new Error(`mpn_divrem_1: qxn must be non-negative integer, got ${qxn}`);
  }
  if (!Number.isInteger(nn) || nn < 0) {
    throw new Error(`mpn_divrem_1: nn must be non-negative integer, got ${nn}`);
  }
  if (np.length < nn) {
    throw new Error(`mpn_divrem_1: np too short`);
  }
  if (typeof d0 !== 'bigint' || d0 <= 0n) {
    throw new Error(`mpn_divrem_1: bad d0`);
  }

  // BUG #1: walk np in BIG-endian (forward) order, treating np[0] as
  // most-significant. Should walk i = nn-1 down to 0.
  let dividend = 0n;
  for (let i = 0; i < nn; i++) {
    const limb = np[i];
    if (limb === undefined) {
      throw new Error(`mpn_divrem_1: undefined limb at index ${i}`);
    }
    dividend = (dividend << LIMB_BITS) | limb;
  }

  // BUG #2: omit the qxn shift. Should be `dividend <<= BigInt(qxn) * LIMB_BITS`.

  const qBig = dividend / d0;
  const r = dividend % d0;

  const qLen = nn + qxn;
  const q: bigint[] = new Array<bigint>(qLen);
  let remaining = qBig;
  for (let i = 0; i < qLen; i++) {
    q[i] = remaining & LIMB_MASK;
    remaining >>= LIMB_BITS;
  }

  return { q: q as readonly bigint[], r };
}
