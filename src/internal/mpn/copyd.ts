/**
 * mpn/copyd.ts -- pure-TS port of GMP's `mpn_copyd`.
 *
 * Substrate-class helper. Copies n limbs from s in decreasing-index
 * order. Operates on raw `bigint[]` limb arrays, not on `MPFR` values.
 *
 * C signature (GMP):
 *
 *   void mpn_copyd(mp_limb_t *rp, const mp_limb_t *sp, mp_size_t n);
 *
 * TS signature (this port):
 *
 *   mpn_copyd(s, n) -> { result: readonly bigint[] }
 *
 * Note: in C, `mpn_copyd` differs from `mpn_copyi` only when the
 * source and destination buffers overlap with `rp > sp` (iterating
 * high-to-low prevents the loop from overwriting limbs before they
 * are read by a later store). In this immutable TS substrate every
 * return is a fresh array, so the iteration direction is invisible
 * to callers -- both routines produce the same output. We keep
 * `mpn_copyd` as a separate export for name-for-name parity with
 * the GMP I/O contract; the canonical MPFR caller (`mpfr_frac` at
 * mpfr/src/frac.c L110) names this primitive specifically.
 *
 * Refs
 * ----
 *
 *   - GMP manual §8.3 (Low-level Functions): decreasing-index copy.
 *   - mpfr/src/frac.c L110 -- canonical caller:
 *       `mpn_copyd (tp + t0, up, un + 1);`
 *   - eval/functions/mpn_copyd/spec.json -- signature contract.
 */

/**
 * Copy `n` limbs from `s` (decreasing-index order; output is identical
 * to `mpn_copyi` because the TS substrate always returns a fresh array).
 *
 * @param s  source limb array; must satisfy `s.length >= n`.
 * @param n  number of limbs to copy; must be `>= 0`.
 * @returns  `{ result }` where `result` is a fresh n-limb array
 *           with the first n limbs of `s`.
 */
export function mpn_copyd(
  s: readonly bigint[],
  n: number,
): { result: readonly bigint[] } {
  if (n < 0) {
    throw new Error(`mpn_copyd: n must be >= 0, got ${n}`);
  }
  if (s.length < n) {
    throw new Error(`mpn_copyd: s.length (${s.length}) < n (${n})`);
  }
  // Ref: GMP manual §8.3 -- decreasing-index copy. Iteration direction
  // is irrelevant for fresh-array semantics; slice yields identical
  // limb data to mpn_copyi for non-overlapping buffers.
  return { result: s.slice(0, n) };
}
