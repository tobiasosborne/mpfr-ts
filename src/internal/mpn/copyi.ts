/**
 * mpn/copyi.ts -- pure-TS port of GMP's `mpn_copyi`.
 *
 * Substrate-class helper. Copies n limbs from s in increasing-index
 * order. Operates on raw `bigint[]` limb arrays, not on `MPFR` values
 * (CLAUDE.md Law 3: faithful substrate, idiomatic surface). The
 * substrate runner exempts this class from `requireCoreImport`, so
 * there is no `src/core.ts` import here.
 *
 * C signature (GMP):
 *
 *   void mpn_copyi(mp_limb_t *rp, const mp_limb_t *sp, mp_size_t n);
 *
 * TS signature (this port):
 *
 *   mpn_copyi(s, n) -> { result: readonly bigint[] }
 *
 * Note: in C, `mpn_copyi` and `mpn_copyd` differ only when the source
 * and destination buffers overlap (increasing vs decreasing iteration
 * direction avoids overwriting limbs before they are read). In this
 * immutable TS substrate every return is a fresh array, so the two
 * routines are functionally equivalent. We keep both as separate
 * exports for name-for-name parity with the GMP I/O contract and the
 * call graph.
 *
 * Refs
 * ----
 *
 *   - GMP manual §8.3 (Low-level Functions): "The least significant
 *     limb is stored at the lowest address (i.e. limbs[0])."
 *   - mpfr/src/mpfr-mini-gmp.c L198, L204, L234, L263 -- C callers.
 *   - eval/functions/mpn_copyi/spec.json -- signature contract.
 */

/**
 * Copy `n` limbs from `s` (increasing-index order).
 *
 * @param s  source limb array; must satisfy `s.length >= n`.
 * @param n  number of limbs to copy; must be `>= 0`.
 * @returns  `{ result }` where `result` is a fresh n-limb array
 *           with the first n limbs of `s`.
 */
export function mpn_copyi(
  s: readonly bigint[],
  n: number,
): { result: readonly bigint[] } {
  if (n < 0) {
    throw new Error(`mpn_copyi: n must be >= 0, got ${n}`);
  }
  if (s.length < n) {
    throw new Error(`mpn_copyi: s.length (${s.length}) < n (${n})`);
  }
  // Ref: GMP manual §8.3 -- increasing-index copy.
  return { result: s.slice(0, n) };
}
