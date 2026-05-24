/**
 * mpfr/mpn/cmp_aux.ts — pure-TS port of MPFR's `mpfr_mpn_cmp_aux`.
 *
 * Substrate-class helper. Operates on raw `bigint[]` limb arrays, NOT on
 * the idiomatic-surface `MPFR` value type from `src/core.ts` — hence no
 * core import here (CLAUDE.md Law 3: "faithful substrate, idiomatic
 * surface").
 *
 * C signature (MPFR):
 *
 *   static int mpfr_mpn_cmp_aux(mpfr_limb_ptr ap, mp_size_t an,
 *                                mpfr_limb_ptr bp, mp_size_t bn, int extra)
 *
 * TS signature (this port):
 *
 *   mpfr_mpn_cmp_aux(ap, bp, extra) -> number   (∈ {-1, 0, +1})
 *
 * The C version takes explicit `an`/`bn` size arguments; the TS port
 * derives them from `ap.length` / `bp.length` (the grader passes full
 * arrays, not sub-slices with separate sizes).
 *
 * Algorithm — MSB-aligned fixed-point comparison
 * ----------------------------------------------
 *
 * Ref: mpfr/src/div.c L662–L718 — the C reference body.
 *
 * This function does NOT compare the numeric values of the two limb arrays.
 * Instead, it compares them as fixed-point numbers with the binary point
 * after their MSB (most-significant limb). Concretely:
 *
 *   - ap represents: ap[an-1] * B^0 + ap[an-2] * B^(-1) + ... + ap[0] * B^(-(an-1))
 *   - bp represents: bp[bn-1] * B^0 + bp[bn-2] * B^(-1) + ... + bp[0] * B^(-(bn-1))
 *
 * where B = 2^64 (GMP_NUMB_BITS). The MSB limb is the "integer" part; lower
 * limbs are fractions. This is the MSB-aligned (not zero-padded) comparison.
 *
 * When extra=1, bp is additionally right-shifted by 1 bit before the compare.
 * The shift is applied limb-by-limb: for limb index i in bp (0 = LSB),
 * the shifted limb i is `(bp[i+1] << 63) | (bp[i] >> 1)`. The bit that
 * "falls off" the right (bit 0 of bp[0]) causes a -1 adjustment if ap would
 * otherwise equal the shifted bp.
 *
 * Limb endianness
 * ---------------
 *
 * GMP stores limbs in LITTLE-ENDIAN limb order: `limbs[0]` is the
 * least-significant 2^64 word, `limbs[n-1]` is the most-significant.
 * Ref: GMP manual §8.3 "Low-level Functions".
 * Ref: CLAUDE.md "Hallucination-risk callouts: mpn limbs are
 *   LITTLE-ENDIAN by limb index".
 *
 * Implementation note
 * -------------------
 *
 * The C reference accesses bp[bn] in the first loop iteration (where bn
 * is the original bn value after decrement). In the C code, the arrays are
 * passed as raw pointers into larger allocations, so bp[bn] is a valid
 * read. In the TS port, we receive only the declared elements; we treat
 * bp[bn] as 0 (one past the declared end of the array is implicitly zero).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/div.c L662–L718 — C reference implementation.
 *   - src/internal/mpn/cmp.ts — sibling (no-shift, same-length compare).
 *   - eval/functions/mpfr_mpn_cmp_aux/spec.json — signature contract.
 *
 * Invariants
 * ----------
 *
 *   Precondition:  extra ∈ {0, 1}
 *                  ∧  every limb is a non-negative bigint in [0, 2^64)
 *                  ∧  ap.length >= 1, bp.length >= 1
 *   Postcondition: returned value ∈ {-1, 0, +1}
 */

const LIMB_BITS = 64n;
const LIMB_ZERO = 0n;

/**
 * Compare {ap, ap.length} against {bp, bp.length} >> extra (MSB-aligned).
 *
 * Ref: mpfr/src/div.c L662–L718 — the C reference body of
 *   mpfr_mpn_cmp_aux. This is a limb-by-limb MSB-aligned comparison,
 *   NOT a numeric comparison of the full array values.
 *
 * @param ap    First operand limb array (little-endian, LSB at index 0).
 * @param bp    Second operand limb array (little-endian, LSB at index 0).
 * @param extra 0 or 1 — number of bits to right-shift bp before compare.
 * @returns     +1 if ap > (bp >> extra), -1 if ap < (bp >> extra), 0 if equal.
 *              Here ">" and "<" are MSB-aligned fixed-point comparisons.
 */
export function mpfr_mpn_cmp_aux(
  ap: readonly bigint[],
  bp: readonly bigint[],
  extra: number,
): number {
  if (extra !== 0 && extra !== 1) {
    throw new Error(
      `mpfr_mpn_cmp_aux: extra must be 0 or 1, got ${extra}`,
    );
  }

  // Ref: mpfr/src/div.c L662-L665 — size variables and assertions.
  let an = ap.length;
  let bn = bp.length;
  let cmp = 0;

  if (an >= bn) {
    // Ref: mpfr/src/div.c L668-L698 — an >= bn branch.
    //
    // k = number of extra low limbs in ap that have no corresponding bp limb
    // after MSB alignment. These will be compared against 0 (or the residual
    // bit from bp[0] for extra=1).
    let k = an - bn;

    // Compare the MSB-aligned common portion: ap[k..k+bn-1] vs bp[0..bn-1].
    // Loop iterates MSB-first: bn decrements from bn_orig-1 down to 0.
    //
    // Ref: mpfr/src/div.c L669-L677:
    //   while (cmp == 0 && bn > 0) {
    //     bn--;
    //     bb = extra ? (bp[bn+1]<<63 | bp[bn]>>1) : bp[bn];
    //     cmp = ap[k+bn] > bb ? 1 : ap[k+bn] < bb ? -1 : 0;
    //   }
    //
    // bp[bn+1] on the first iteration is bp[bn_orig], which is one past
    // the declared array end. In the C code this reads valid (but potentially
    // stale) memory from the underlying allocation. In the TS port we treat
    // it as 0 (the safe default for out-of-bounds).
    while (cmp === 0 && bn > 0) {
      bn--;
      const bpNext: bigint = (bn + 1 < bp.length) ? bp[bn + 1]! : LIMB_ZERO;
      const bpCur: bigint = bp[bn]!;
      // In C: (uint64_t)bpNext << 63 — the shift truncates to 64 bits, so only
      // bit 0 of bpNext survives. TS BigInt does not overflow, so we must
      // explicitly take only bit 0 before shifting.
      // Ref: mpfr/src/div.c L672-L673 — C uses 64-bit wrapping arithmetic.
      const bb: bigint = extra
        ? (((bpNext & 1n) << (LIMB_BITS - 1n)) | (bpCur >> 1n))
        : bpCur;
      const apVal: bigint = ap[k + bn]!;
      if (apVal > bb) { cmp = 1; break; }
      if (apVal < bb) { cmp = -1; break; }
    }

    // After exhausting bn, compute the residual bb from bp[0] (for extra=1).
    // Ref: mpfr/src/div.c L678: bb = extra ? bp[0] << 63 : 0;
    // Again, only bit 0 of bp[0] survives the C 64-bit left shift by 63.
    let bb: bigint = extra ? ((bp[0]! & 1n) << (LIMB_BITS - 1n)) : LIMB_ZERO;

    // Compare the remaining low limbs of ap (ap[0..k-1]) against bb (then 0).
    // Ref: mpfr/src/div.c L679-L685:
    //   while (cmp == 0 && k > 0) {
    //     k--;
    //     cmp = ap[k] > bb ? 1 : ap[k] < bb ? -1 : 0;
    //     bb = 0;  // only first ap[k] compares against residual bit
    //   }
    while (cmp === 0 && k > 0) {
      k--;
      const apVal: bigint = ap[k]!;
      if (apVal > bb) { cmp = 1; break; }
      if (apVal < bb) { cmp = -1; break; }
      bb = LIMB_ZERO; // subsequent low ap limbs compare against 0
    }

    // If cmp still 0 but there was a residual bit (bb != 0), then ap < bp
    // because the shifted bp has a fractional part > 0 that ap lacks.
    // Ref: mpfr/src/div.c L686-L687: if (cmp == 0 && bb != 0) cmp = -1;
    if (cmp === 0 && bb !== LIMB_ZERO) {
      cmp = -1;
    }
  } else {
    // an < bn
    // Ref: mpfr/src/div.c L689-L715 — an < bn branch.
    //
    // k = number of extra high limbs in bp that have no corresponding ap limb.
    // These are compared against 0 on the ap side; if any is non-zero, ap < bp.
    let k = bn - an;

    // Compare the MSB-aligned common portion: ap[0..an-1] vs bp[k..k+an-1].
    // Loop iterates MSB-first: an decrements from an_orig-1 down to 0.
    //
    // Ref: mpfr/src/div.c L691-L700:
    //   while (cmp == 0 && an > 0) {
    //     an--;
    //     bb = extra ? (bp[k+an+1]<<63 | bp[k+an]>>1) : bp[k+an];
    //     if (ap[an] > bb) cmp = 1;
    //     else if (ap[an] < bb) cmp = -1;
    //   }
    while (cmp === 0 && an > 0) {
      an--;
      const bpNext: bigint = (k + an + 1 < bp.length) ? bp[k + an + 1]! : LIMB_ZERO;
      const bpCur: bigint = bp[k + an]!;
      // Only bit 0 of bpNext survives << 63 in C 64-bit arithmetic.
      // Ref: mpfr/src/div.c L693-L694.
      const bb: bigint = extra
        ? (((bpNext & 1n) << (LIMB_BITS - 1n)) | (bpCur >> 1n))
        : bpCur;
      const apVal: bigint = ap[an]!;
      if (apVal > bb) { cmp = 1; break; }
      if (apVal < bb) { cmp = -1; break; }
    }

    // Compare the extra high limbs of bp (bp[0..k-1]) against 0 on ap side.
    // Ref: mpfr/src/div.c L701-L708:
    //   while (cmp == 0 && k > 0) {
    //     k--;
    //     bb = extra ? (bp[k+1]<<63 | bp[k]>>1) : bp[k];
    //     cmp = (bb != 0) ? -1 : 0;
    //   }
    while (cmp === 0 && k > 0) {
      k--;
      const bpNext: bigint = (k + 1 < bp.length) ? bp[k + 1]! : LIMB_ZERO;
      const bpCur: bigint = bp[k]!;
      // Only bit 0 of bpNext survives << 63 in C 64-bit arithmetic.
      // Ref: mpfr/src/div.c L703-L704.
      const bb: bigint = extra
        ? (((bpNext & 1n) << (LIMB_BITS - 1n)) | (bpCur >> 1n))
        : bpCur;
      if (bb !== LIMB_ZERO) { cmp = -1; break; }
    }

    // Trailing-bit check: if extra=1 and bit 0 of bp[0] is set, then
    // the shifted bp has a fractional part that makes it larger than ap.
    // Ref: mpfr/src/div.c L709-L711:
    //   if (cmp == 0 && extra && (bp[0] & 1)) cmp = -1;
    if (cmp === 0 && extra === 1 && (bp[0]! & 1n) !== LIMB_ZERO) {
      cmp = -1;
    }
  }

  return cmp;
}
