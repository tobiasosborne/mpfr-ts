/**
 * mpn/add_1.ts -- pure-TS port of GMP's `mpn_add_1`.
 *
 * Substrate-class helper. Adds a single-limb scalar `x` to the n-limb
 * non-negative integer `s[0..n)`, returning the n-limb sum and the
 * carry-out (0 or 1). Operates on raw `bigint[]` limb arrays, not on
 * `MPFR` values (CLAUDE.md Law 3: faithful substrate, idiomatic
 * surface). The runner exempts substrate from `requireCoreImport`, so
 * there is no `src/core.ts` import here.
 *
 * C signature (GMP):
 *
 *   mp_limb_t mpn_add_1(mp_limb_t *rp,
 *                       const mp_limb_t *sp,
 *                       mp_size_t n,
 *                       mp_limb_t x);
 *
 * TS signature (this port):
 *
 *   mpn_add_1(s, n, x) -> { result, carry }
 *
 * Algorithm
 * ---------
 *
 *   sum_0       = s[0] + x
 *   result[0]   = sum_0 mod 2^64
 *   carry_0     = sum_0 div 2^64
 *
 *   for i in 1..n-1:
 *     s_i       = s[i] + carry_{i-1}
 *     result[i] = s_i mod 2^64
 *     carry_i   = s_i div 2^64
 *
 *   return { result, carry_{n-1} }
 *
 * The carry chain may short-circuit once an intermediate carry becomes
 * 0n (no further limb can produce a carry), but the explicit loop is
 * already O(1) in the no-carry case because BigInt arithmetic doesn't
 * pay limb-level overhead. We keep the loop straightforward.
 *
 * Edge case n=0
 * -------------
 *
 * GMP's `mpn_add_1` with `n=0` writes nothing to `rp` and returns
 * `0` regardless of `x`. The scalar `x` is silently discarded when
 * there are no limbs to add into -- the carry-out is structural,
 * not value-passing. (Empirically verified against libgmp; case 21
 * of the golden: `mpn_add_1([], 0, 1)` -> carry=0, not 1.) The TS
 * port follows the same contract:
 * `mpn_add_1(s, 0, x) -> { result: [], carry: 0n }` for any `x`.
 *
 * Why carry is always 0n or 1n (after the first limb)
 * ----------------------------------------------------
 *
 * After `i = 0`, the carry is `0n` or `1n` because `s[0] + x <
 * 2*(2^64 - 1) + 1 = 2^65 - 1`, so `(s[0] + x) >> 64n in {0, 1}`. For
 * `i >= 1`, `s[i] + carry_{i-1} <= (2^64 - 1) + 1 = 2^64`, so the
 * carry is `0n` or `1n`. The structural guarantee removes the need
 * for a runtime check on the returned carry value.
 *
 * Limb endianness
 * ---------------
 *
 * GMP stores limbs in LITTLE-ENDIAN limb order: `s[0]` is the
 * least-significant 2^64 word, `s[n-1]` is the most-significant. The
 * carry chain propagates from index 0 upward.
 *
 * Refs
 * ----
 *
 *   - GMP manual §8.3 (Low-level Functions): single-limb add with
 *     carry-out.
 *   - mpfr/src/add1.c, mpfr/src/add1sp.c -- significand += 1 ulp.
 *   - mpfr/src/div.c -- division-correction loop.
 *   - eval/functions/mpn_add_1/spec.json -- signature contract,
 *     limb_width_bits: 64, limb_order: "little-endian".
 */

const LIMB_BITS = 64n;
const LIMB_BASE = 1n << LIMB_BITS;
const LIMB_MASK = LIMB_BASE - 1n;

/**
 * Add `x` to the n-limb integer `s` and return the n-limb sum plus the
 * carry-out (0n or 1n; equals `x` when n is 0).
 *
 * @param s  source limb array; must satisfy `s.length >= n`.
 * @param n  number of limbs; must be `>= 0`.
 * @param x  single-limb scalar to add; must be in `[0, 2^64)`.
 * @returns  `{ result, carry }` where `result` is a fresh n-limb array
 *           and `carry` is the carry-out of the high limb (or `x` when
 *           `n === 0`).
 */
export function mpn_add_1(
  s: readonly bigint[],
  n: number,
  x: bigint,
): { result: readonly bigint[]; carry: bigint } {
  if (n < 0) {
    throw new Error(`mpn_add_1: n must be >= 0, got ${n}`);
  }
  if (s.length < n) {
    throw new Error(`mpn_add_1: s.length (${s.length}) < n (${n})`);
  }
  if (typeof x !== 'bigint') {
    throw new Error(`mpn_add_1: x must be bigint, got ${typeof x}`);
  }
  if (x < 0n || x >= LIMB_BASE) {
    throw new Error(`mpn_add_1: x must be in [0, 2^64), got ${x}`);
  }

  // Ref: GMP behaviour -- with n=0, x is silently discarded and carry=0.
  // Empirically verified against libgmp (case 21 of the golden).
  if (n === 0) {
    return { result: [], carry: 0n };
  }

  // Ref: GMP manual §8.3 -- carry chain propagates LSB-first.
  const result: bigint[] = new Array<bigint>(n);
  let carry: bigint = x;
  for (let i = 0; i < n; i++) {
    const sum = s[i]! + carry;
    result[i] = sum & LIMB_MASK;
    carry = sum >> LIMB_BITS;
  }
  return { result, carry };
}
