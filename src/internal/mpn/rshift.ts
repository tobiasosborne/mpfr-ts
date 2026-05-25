/**
 * mpn/rshift.ts -- pure-TS port of GMP's `mpn_rshift`.
 *
 * Substrate-class helper. Shifts an n-limb non-negative integer (little-
 * endian limb order) right by `count` bits, returning the n-limb result
 * plus the `count` bits that fell off the bottom, packed into the HIGH
 * `count` bits of the returned `out` limb. Symmetric to mpn_lshift.
 *
 * C signature (GMP):
 *
 *   mp_limb_t mpn_rshift(mp_limb_t *rp, const mp_limb_t *sp,
 *                        mp_size_t n, unsigned int count);
 *
 * TS signature (this port):
 *
 *   mpn_rshift(s, n, count) -> { result, out }
 *
 * Algorithm
 * ---------
 *
 * For each limb index `i` in `[0, n)`:
 *
 *   next      = (i + 1 < n) ? s[i+1] : 0n
 *   result[i] = (s[i] >> count) | ((next << (LIMB_BITS - count)) & MASK)
 *
 * That is, the high (64-count) bits of result[i] are the low (64-count)
 * bits of s[i]; the low count bits of result[i] are the low count bits
 * of s[i+1] (which were the low bits of the next-higher input limb,
 * now shifted down into position).
 *
 * The "shifted out" return value is the low `count` bits of `s[0]`,
 * relocated to the HIGH `count` bits of the returned mp_limb_t:
 *
 *   out = (s[0] << (LIMB_BITS - count)) & LIMB_MASK
 *
 * Equivalent to `(s[0] & ((1n << count) - 1n)) << (LIMB_BITS - count)`.
 *
 * BigInt masking is LOAD-BEARING: without `& LIMB_MASK`, bits that
 * should have been discarded into the void by the C `mp_limb_t` wrap
 * remain in the bigint. See the symmetric load-bearing-mask note in
 * src/internal/mpn/lshift.ts.
 *
 * Limb endianness
 * ---------------
 *
 * GMP stores limbs LITTLE-ENDIAN: `s[0]` is the LSB. Right-shifting an
 * n-limb integer moves bits DOWN, so the bits exiting limb 0's low end
 * are what mpn_rshift returns. The C in-place rp == sp case requires
 * LSB-first iteration (read each limb before its low neighbours are
 * overwritten); the fresh-output TS port has no aliasing so iteration
 * direction is irrelevant for correctness.
 *
 * Refs
 * ----
 *
 *   - GMP manual §8.3 (Low-level Functions): "Shift {sp, n} right by
 *     count bits ... The bits shifted out at the bottom are returned
 *     in the high count bits of the return value."
 *   - mpfr/src/div.c, mpfr/src/get_str.c -- canonical MPFR callers.
 *   - src/internal/mpn/lshift.ts -- symmetric primitive.
 *   - eval/functions/mpn_rshift/spec.json -- signature contract.
 *
 * Invariants
 * ----------
 *
 *   Precondition:  s.length >= n  AND  n >= 1  AND  1 <= count <= 63
 *                  AND every limb is a non-negative bigint < 2^64.
 *
 *   Postcondition: returned `result.length === n`,
 *                  every result limb in [0, 2^64),
 *                  returned `out` in [0, 2^64) -- the low `count`
 *                  bits of s[0] shifted to occupy the high `count`
 *                  bits of out.
 */

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

export interface MpnRshiftResult {
  readonly result: readonly bigint[];
  readonly out: bigint;
}

/**
 * Right-shift an n-limb integer by `count` bits.
 *
 * @param s      input limb array; must have `length >= n`.
 * @param n      number of limbs; must be `>= 1`.
 * @param count  shift amount; must be in `[1, 63]`.
 * @returns      `{ result, out }`.
 *
 * @throws  `Error` on domain violation (n < 1, count outside [1,63],
 *           s.length < n).
 */
export function mpn_rshift(
  s: readonly bigint[],
  n: number,
  count: number,
): MpnRshiftResult {
  if (!Number.isInteger(n) || n < 1) {
    throw new Error(`mpn_rshift: n must be a positive integer, got ${n}`);
  }
  if (!Number.isInteger(count) || count < 1 || count > 63) {
    throw new Error(
      `mpn_rshift: count must be an integer in [1, 63], got ${count}`,
    );
  }
  if (s.length < n) {
    throw new Error(
      `mpn_rshift: s too short: need ${n} limbs, got ${s.length}`,
    );
  }

  const countBig = BigInt(count);
  const complBig = LIMB_BITS - countBig; // 64 - count, in [1, 63]

  const result: bigint[] = new Array<bigint>(n);
  for (let i = 0; i < n; i++) {
    const cur = s[i]!;
    const next = i + 1 < n ? s[i + 1]! : 0n;
    // Ref: GMP manual §8.3 -- (cur >> count) | (next.low_count_bits in cur's high count).
    result[i] = (cur >> countBig) | ((next << complBig) & LIMB_MASK);
  }

  // Ref: GMP manual §8.3 -- shifted-out bits returned in HIGH count bits.
  const out = (s[0]! << complBig) & LIMB_MASK;

  return { result: result as readonly bigint[], out };
}
