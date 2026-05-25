/**
 * mpn/zero.ts -- pure-TS port of GMP's `mpn_zero`.
 *
 * Substrate-class helper. Produces an n-limb all-zeros array.
 *
 * C signature (GMP):
 *
 *   void mpn_zero(mp_limb_t *rp, mp_size_t n);
 *
 * TS signature (this port):
 *
 *   mpn_zero(n) -> { result: readonly bigint[] }
 *
 * Refs
 * ----
 *
 *   - GMP manual §8.3 (Low-level Functions): zero-fill n limbs.
 *   - mpfr/src/mpfr-mini-gmp.c L199, L206, L237, L239, L265, L269 --
 *     C callers inside mpn_divrem_1 / mpn_tdiv_qr.
 *   - eval/functions/mpn_zero/spec.json -- signature contract.
 */

/**
 * Produce a fresh n-limb array of zeros.
 *
 * @param n  number of zero limbs; must be `>= 0`.
 * @returns  `{ result }` where `result` is a fresh n-limb array
 *           with every entry set to `0n`.
 */
export function mpn_zero(n: number): { result: readonly bigint[] } {
  if (n < 0) {
    throw new Error(`mpn_zero: n must be >= 0, got ${n}`);
  }
  // Ref: GMP manual §8.3 -- n-limb zero fill.
  return { result: new Array<bigint>(n).fill(0n) };
}
