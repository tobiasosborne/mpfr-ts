/**
 * mpfr/sub_aux.ts — pure-TS port of MPFR's `mpfr_mpn_sub_aux`.
 *
 * Substrate-class helper. Operates on raw `bigint[]` limb arrays, NOT on
 * the idiomatic-surface `MPFR` value type from `src/core.ts` — hence no
 * core import here (CLAUDE.md Law 3: "faithful substrate, idiomatic
 * surface").
 *
 * C signature (MPFR):
 *
 *   static mp_limb_t mpfr_mpn_sub_aux(mpfr_limb_ptr ap,
 *                                      mpfr_limb_ptr bp,
 *                                      mp_size_t n,
 *                                      mp_limb_t cy,
 *                                      int extra);
 *
 * TS signature (this port):
 *
 *   mpfr_mpn_sub_aux(ap, bp, cy, extra) -> { result, borrow }
 *
 * The C version takes `n` (the number of limbs in `ap`) implicitly from
 * `ap.length`. The TS port derives `n = ap.length` from the input array.
 * The C version mutates `ap` in place; the TS port returns a fresh
 * `result` array with the modified ap values, and `borrow` as the borrow-out.
 *
 * Algorithm
 * ---------
 *
 * Ref: mpfr/src/div.c L723-L744 — the C reference body.
 *
 * The function computes:
 *
 *     {ap, n} <- {ap, n} - ({bp, n} >> extra) - cy
 *
 * where `extra` is 0 or 1, `cy` is the initial carry-in (0 or 1).
 *
 * For each limb index `i` in `[0, n)`:
 *
 *   If extra == 0:
 *     bb = bp[i]
 *   If extra == 1:
 *     bb = (bp[i+1] << 63) | (bp[i] >> 1)
 *       (i.e., the shifted b value uses bit 0 of bp[i+1] as the high bit
 *        of the shifted bp[i], mirroring a right-shift of the whole array
 *        by 1 bit)
 *
 *   rp = ap[i] - bb - cy
 *   result[i] = rp mod 2^64
 *   cy = 1 if (ap[i] < bb) || (cy was 1 && rp == 2^64-1) else 0
 *       (borrow-out condition from mpfr/src/div.c L733-L735)
 *
 * The borrow condition mirrors the C exactly:
 *   cy = (ap[0] < bb) || (cy && rp == MPFR_LIMB_MAX)
 * where MPFR_LIMB_MAX = 2^64 - 1 (all-ones 64-bit value).
 *
 * Note: `rp` in the borrow check is the WRAPPED (mod 2^64) value, not
 * the raw signed difference. Since BigInt doesn't overflow, we must
 * compute the wrapped value explicitly to replicate the C borrow logic.
 *
 * Limb endianness
 * ---------------
 *
 * GMP stores limbs in LITTLE-ENDIAN limb order: `limbs[0]` is the
 * least-significant 2^64 word, `limbs[n-1]` is the most-significant.
 * The borrow chain propagates from index 0 upward.
 *
 * Ref: CLAUDE.md "Hallucination-risk callouts: GMP mpn limbs are
 *   LITTLE-ENDIAN by limb index".
 *
 * When extra == 1, the shifted b at limb index i uses:
 *   - bp[i+1]'s bit 0 (LSB) as the MSB of the shifted value
 *   - bp[i] right-shifted by 1 for the remaining bits
 * This correctly implements a 1-bit right shift of the entire bp array.
 *
 * Invariants
 * ----------
 *
 *   Precondition:
 *     - ap.length >= 1 (n = ap.length)
 *     - When extra == 1: bp.length >= n + 1 (needs one lookahead limb)
 *     - When extra == 0: bp.length >= n
 *     - cy in {0n, 1n}
 *     - extra in {0, 1}
 *
 *   Postcondition:
 *     - result.length === n
 *     - every result limb is in [0, 2^64)
 *     - borrow in {0n, 1n}
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;
const LIMB_MAX = LIMB_MASK; // 0xFFFFFFFFFFFFFFFFn

export interface MpfrMpnSubAuxResult {
  readonly result: readonly bigint[];
  readonly borrow: bigint;
}

/**
 * Subtract ({bp, n} >> extra) plus carry-in cy from {ap, n}, and return
 * the modified array (as `result`) plus the borrow-out (as `borrow`).
 *
 * @param ap    Minuend limb array; `n = ap.length` limbs processed.
 * @param bp    Subtrahend limb array; must have length >= n (extra==0) or
 *              >= n+1 (extra==1).
 * @param cy    Carry-in; must be 0n or 1n.
 * @param extra Shift amount; must be 0 or 1.
 * @returns {result, borrow} where result.length === ap.length and
 *          borrow in {0n, 1n}.
 *
 * @throws Error on invalid inputs (substrate uses plain Error, not MPFRError).
 */
export function mpfr_mpn_sub_aux(
  ap: readonly bigint[],
  bp: readonly bigint[],
  cy: bigint,
  extra: number,
): MpfrMpnSubAuxResult {
  const n = ap.length;

  if (extra !== 0 && extra !== 1) {
    throw new Error(`mpfr_mpn_sub_aux: extra must be 0 or 1, got ${extra}`);
  }
  if (cy !== 0n && cy !== 1n) {
    throw new Error(`mpfr_mpn_sub_aux: cy must be 0n or 1n, got ${cy}`);
  }
  // When extra == 1, we need bp[i+1] for each i in [0, n), so bp.length >= n+1.
  // When extra == 0, we need bp[i] for each i in [0, n), so bp.length >= n.
  const requiredBpLen = extra ? n + 1 : n;
  if (bp.length < requiredBpLen) {
    throw new Error(
      `mpfr_mpn_sub_aux: bp too short: need ${requiredBpLen} limbs (n=${n}, extra=${extra}), got ${bp.length}`,
    );
  }

  const result: bigint[] = new Array<bigint>(n);
  let cyOut: bigint = cy;

  // Ref: mpfr/src/div.c L730-L738 — the inner loop body, mirrored verbatim.
  //
  // C code:
  //   while (n--)
  //   {
  //     bb = (extra) ? (MPFR_LIMB_LSHIFT(bp[1],GMP_NUMB_BITS-1) | (bp[0] >> 1))
  //                  : bp[0];
  //     rp = ap[0] - bb - cy;
  //     cy = (ap[0] < bb) || (cy && rp == MPFR_LIMB_MAX) ?
  //       MPFR_LIMB_ONE : MPFR_LIMB_ZERO;
  //     ap[0] = rp;
  //     ap++;
  //     bp++;
  //   }
  //
  // Note: in C, ap and bp are pointer-advanced each iteration. In TS we
  // use index `i` instead. At iteration i, C's `ap[0]` = our `ap[i]`,
  // C's `bp[0]` = our `bp[i]`, C's `bp[1]` = our `bp[i+1]`.
  //
  // MPFR_LIMB_LSHIFT(x, c) = x << c (for 64-bit limbs, this is a left
  // shift; we mask to 64 bits since BigInt doesn't overflow).
  // GMP_NUMB_BITS - 1 = 63.
  // MPFR_LIMB_MAX = 2^64 - 1.

  for (let i = 0; i < n; i++) {
    const aLimb = ap[i];
    const bCurr = bp[i];
    const bNext = extra ? bp[i + 1] : undefined;

    if (aLimb === undefined || bCurr === undefined) {
      throw new Error(
        `mpfr_mpn_sub_aux: internal invariant violated — undefined limb at index ${i}`,
      );
    }
    if (extra && bNext === undefined) {
      throw new Error(
        `mpfr_mpn_sub_aux: internal invariant violated — undefined bp[${i + 1}] for extra=1`,
      );
    }

    // Compute bb: the shifted subtrahend limb.
    // When extra == 1:
    //   bb = (bp[i+1] << 63) & LIMB_MASK | (bp[i] >> 1)
    //   This takes bit 0 of bp[i+1] as the MSB of bb, and bp[i] >> 1 as lower 63 bits.
    // When extra == 0:
    //   bb = bp[i]
    let bb: bigint;
    if (extra) {
      // (bp[i+1] << 63) gives the high bit of bb; mask to 64 bits.
      // (bp[i] >> 1) gives the lower 63 bits.
      bb = ((bNext! << 63n) & LIMB_MASK) | (bCurr >> 1n);
    } else {
      bb = bCurr;
    }

    // rp = ap[i] - bb - cy (in C, this wraps modulo 2^64 via unsigned arithmetic)
    // In BigInt, compute the wrapped value explicitly.
    const cyIn = cyOut;
    const rawDiff = aLimb - bb - cyIn;
    const rp = rawDiff & LIMB_MASK; // wrapped mod 2^64

    // Borrow condition (from C):
    //   cy = (ap[0] < bb) || (cy && rp == MPFR_LIMB_MAX)
    // Note: `rp` here is the wrapped value (C's `rp` is the unsigned wrap).
    const newCy =
      aLimb < bb || (cyIn === 1n && rp === LIMB_MAX) ? 1n : 0n;

    result[i] = rp;
    cyOut = newCy;
  }

  return { result: result as readonly bigint[], borrow: cyOut };
}
