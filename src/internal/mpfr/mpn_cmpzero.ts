/**
 * mpn/cmpzero.ts — pure-TS port of MPFR's `mpfr_mpn_cmpzero`.
 *
 * Substrate-class helper. Operates on raw `bigint[]` limb arrays. No
 * import from `src/core.ts` (CLAUDE.md Law 3: faithful substrate, idiomatic
 * surface). The substrate split is load-bearing.
 *
 * C signature (MPFR, mpfr/src/div.c L648–L656):
 *
 *   static int mpfr_mpn_cmpzero(mpfr_limb_ptr ap, mp_size_t an)
 *   {
 *     MPFR_ASSERTD (an >= 0);
 *     while (an > 0)
 *       if (MPFR_LIKELY(ap[--an] != MPFR_LIMB_ZERO))
 *         return 1;
 *     return 0;
 *   }
 *
 * TS signature (this port):
 *
 *   mpfr_mpn_cmpzero(ap: readonly bigint[]) -> number
 *
 * The C version takes an explicit length `an`; the TS port uses `ap.length`
 * (the spec.json signature is `(ap) -> number`, length implicit).
 *
 * Algorithm
 * ---------
 *
 * Scan the limb array for any non-zero limb. The C reference iterates from
 * the most-significant limb downward (index `an-1` to `0`) as a
 * likely-non-zero short-circuit heuristic — this matches MPFR's comment
 * "MPFR_LIKELY" for the non-zero case. The TS port uses `Array.prototype.some`
 * which short-circuits on the first truthy element. We iterate from the high
 * end to match the C iteration order exactly, preserving the same
 * short-circuit semantics.
 *
 * Limb endianness
 * ---------------
 *
 * GMP limbs are LITTLE-ENDIAN by limb index: `ap[0]` is the
 * least-significant 64-bit word, `ap[an-1]` is the most-significant.
 * For this function (zero-test only), the iteration direction does not
 * affect correctness — any non-zero limb produces the same result
 * regardless of which end we start from. We match the C direction
 * (high-to-low) as a fidelity discipline.
 *
 * Ref: mpfr/src/div.c L648–L656 — the C reference body.
 * Ref: CLAUDE.md "Hallucination-risk callouts: GMP mpn limbs are
 *   LITTLE-ENDIAN by limb index".
 *
 * Invariants
 * ----------
 *
 *   Precondition:  ap.length >= 0
 *                  Each ap[i] is a bigint in [0, 2^64).
 *
 *   Postcondition: returns 1 if any ap[i] !== 0n, else 0.
 */

/**
 * Test whether the limb array `ap` is all-zero.
 *
 * Mirrors `mpfr_mpn_cmpzero(ap, ap.length)` from `mpfr/src/div.c L648–L656`.
 *
 * @param ap  Limb array (little-endian, 64-bit limbs as bigint). May be empty.
 * @returns   `1` if any limb is non-zero; `0` if all limbs are zero (or the
 *            array is empty).
 */
export function mpfr_mpn_cmpzero(ap: readonly bigint[]): number {
  // Ref: mpfr/src/div.c L648-L656 — C iterates from an-1 down to 0,
  // returning 1 on the first non-zero limb. We do the same with a
  // reverse-direction scan to match C fidelity.
  for (let i = ap.length - 1; i >= 0; i--) {
    if (ap[i] !== 0n) {
      return 1;
    }
  }
  return 0;
}
