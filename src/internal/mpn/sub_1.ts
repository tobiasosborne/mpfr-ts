/**
 * mpn/sub_1.ts -- pure-TS port of GMP's `mpn_sub_1`.
 *
 * Substrate-class helper. Subtracts a single-limb scalar `x` from the
 * n-limb non-negative integer `s[0..n)`, returning the n-limb difference
 * and the borrow-out (0 or 1). Symmetric to mpn_add_1.
 *
 * C signature (GMP):
 *
 *   mp_limb_t mpn_sub_1(mp_limb_t *rp, const mp_limb_t *sp,
 *                       mp_size_t n, mp_limb_t x);
 *
 * TS signature (this port):
 *
 *   mpn_sub_1(s, n, x) -> { result, borrow }
 *
 * Algorithm
 * ---------
 *
 *   diff_0       = s[0] - x
 *   result[0]    = diff_0 mod 2^64       (two's-complement wrap)
 *   borrow_0     = diff_0 < 0n ? 1n : 0n
 *
 *   for i in 1..n-1:
 *     d_i        = s[i] - borrow_{i-1}
 *     result[i]  = d_i mod 2^64
 *     borrow_i   = d_i < 0n ? 1n : 0n
 *
 *   return { result, borrow_{n-1} }
 *
 * BigInt wrapping
 * ---------------
 *
 * `(d & LIMB_MASK)` reduces a possibly-negative BigInt to its low 64
 * bits. For any negative `d in [-2^64, -1]`:
 *
 *   (d & ((1n << 64n) - 1n)) === d + 2^64   (mod 2^64)
 *
 * which is precisely the limb GMP's C macro writes via the unsigned-
 * arithmetic wrap. Mirrors the borrow convention from
 * src/internal/mpn/sub_n.ts.
 *
 * Edge case n=0
 * -------------
 *
 * GMP's `mpn_sub_1` with n=0 returns `borrow = (x != 0 ? 1 : 0)`.
 * (Asymmetric with mpn_add_1's discard behavior: subtraction with
 * positive x from "nothing" is conceptually an underflow, so the
 * borrow flag is set; addition of any x to "nothing" has nowhere
 * for a carry to go and returns 0.) Verified empirically -- golden
 * case 21: `mpn_sub_1([], 0, 1)` -> borrow=1.
 *
 * Refs
 * ----
 *
 *   - GMP manual §8.3 (Low-level Functions): single-limb subtract.
 *   - mpfr/src/sub1.c -- canonical caller.
 *   - src/internal/mpn/add_1.ts -- symmetric primitive.
 *   - src/internal/mpn/sub_n.ts -- borrow-chain convention.
 */

const LIMB_BITS = 64n;
const LIMB_BASE = 1n << LIMB_BITS;
const LIMB_MASK = LIMB_BASE - 1n;

/**
 * Subtract `x` from the n-limb integer `s` and return the difference.
 *
 * @param s  source limb array; must satisfy `s.length >= n`.
 * @param n  number of limbs; must be `>= 0`.
 * @param x  single-limb scalar; must be in `[0, 2^64)`.
 * @returns  `{ result, borrow }`.
 */
export function mpn_sub_1(
  s: readonly bigint[],
  n: number,
  x: bigint,
): { result: readonly bigint[]; borrow: bigint } {
  if (n < 0) {
    throw new Error(`mpn_sub_1: n must be >= 0, got ${n}`);
  }
  if (s.length < n) {
    throw new Error(`mpn_sub_1: s.length (${s.length}) < n (${n})`);
  }
  if (typeof x !== 'bigint') {
    throw new Error(`mpn_sub_1: x must be bigint, got ${typeof x}`);
  }
  if (x < 0n || x >= LIMB_BASE) {
    throw new Error(`mpn_sub_1: x must be in [0, 2^64), got ${x}`);
  }

  // Ref: GMP -- n=0 returns borrow=1 if x>0, else 0 (asymmetric with
  // mpn_add_1; case 21 of the golden empirically verifies).
  if (n === 0) {
    return { result: [], borrow: x === 0n ? 0n : 1n };
  }

  // Ref: GMP manual §8.3 -- borrow chain propagates LSB-first.
  const result: bigint[] = new Array<bigint>(n);
  let borrow: bigint = x;
  for (let i = 0; i < n; i++) {
    const diff = s[i]! - borrow;
    result[i] = diff & LIMB_MASK;
    borrow = diff < 0n ? 1n : 0n;
  }
  return { result, borrow };
}
