/**
 * mpfr/powerof2_raw2.ts — pure-TS port of MPFR's `mpfr_powerof2_raw2`.
 *
 * Substrate-class helper. Operates on raw bigint mantissa values — NOT on
 * the idiomatic-surface `MPFR` value type from `src/core.ts` — hence no
 * core import here (CLAUDE.md Law 3: "faithful substrate, idiomatic surface").
 *
 * C signature (mpfr/src/powerof2.c L40-L49):
 *
 *   int mpfr_powerof2_raw2(const mp_limb_t *xp, mp_size_t xn);
 *
 * TS signature (this port):
 *
 *   mpfr_powerof2_raw2(mant: bigint, prec: bigint): boolean
 *
 * Algorithm
 * ---------
 *
 * The C function checks:
 *   1. The top limb (xp[xn-1]) equals MPFR_LIMB_HIGHBIT (= 1 << 63 for 64-bit).
 *   2. All lower limbs (xp[0..xn-2]) are zero.
 *
 * In the locked TS schema, `mant` is a single bigint MSB-aligned to `prec`
 * bits. The condition "only the MSB is set" translates directly to:
 *
 *   mant === 1n << (prec - 1n)
 *
 * This is bit-for-bit equivalent to the C limb-walk at the I/O contract
 * level — the bigint encodes the same bit pattern as the limb array; the
 * MSB position is `prec - 1` regardless of how many limbs the C side uses.
 *
 * Ref: mpfr/src/powerof2.c L40-L49 — the C reference body.
 * Ref: mpfr/src/mpfr-impl.h L1301 — MPFR_LIMB_HIGHBIT = 1 << (GMP_NUMB_BITS-1).
 * Ref: src/core.ts L93-L135 — locked MPFR value model; mant MSB-aligned to prec bits.
 * Ref: CLAUDE.md Law 3 — faithful substrate, idiomatic surface.
 *
 * Callers
 * -------
 *
 * The C-side caller `mpfr_powerof2_raw` (L30-L38) invokes this only after
 * asserting `MPFR_IS_PURE_FP(x)` (even if that ASSERTN is disabled, the
 * contract is documented). The TS port mirrors: callers should only invoke
 * this on a `mant` drawn from a `'normal'`-kind MPFR value where the
 * MSB-alignment invariant (`mant >= 2n^(prec-1)`) holds. An out-of-range
 * or zero `mant` silently returns false (same as the C function).
 *
 * No exceptions are thrown; this is a pure predicate.
 */

/**
 * Returns true iff the mantissa `mant` (a bigint MSB-aligned to `prec` bits)
 * represents a power of 2 — i.e. exactly the MSB at position `prec-1` is set
 * and all lower bits are zero.
 *
 * Faithful TS port of `mpfr_powerof2_raw2` from mpfr/src/powerof2.c L40-L49.
 * The C function checks `xp[xn-1] === MPFR_LIMB_HIGHBIT` and all lower limbs
 * are zero; the bigint expression `mant === 1n << (prec - 1n)` is equivalent.
 *
 * @param mant  Mantissa bigint, MSB-aligned to `prec` bits (i.e. bit `prec-1`
 *              is expected to be set for normal MPFR values). No bounds check.
 * @param prec  Precision in bits (>= 1 for well-formed normal MPFR values).
 * @returns     `true` iff `mant` has exactly one bit set (the MSB).
 *
 * @mpfrName mpfr_powerof2_raw2
 */
export function mpfr_powerof2_raw2(mant: bigint, prec: bigint): boolean {
  // Ref: mpfr/src/powerof2.c L40-L49
  //   C: check top limb === MPFR_LIMB_HIGHBIT AND all lower limbs === 0.
  //   TS: equivalent bigint predicate — only the MSB at position prec-1 is set.
  return mant === (1n << (prec - 1n));
}
