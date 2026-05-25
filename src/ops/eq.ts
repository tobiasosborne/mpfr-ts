/**
 * ops/eq.ts -- pure-TS port of MPFR's `mpfr_eq`.
 *
 * Compare two MPFR values to the first `n_bits` bits of precision. This
 * is the legacy GMP `mpf_eq` compatibility shim (the modern recommendation
 * is `mpfr_cmp` + `mpfr_get_prec`); kept for API parity with libmpfr.
 *
 * Algorithm (mpfr/src/eq.c L27-L140), simplified by the MSB-aligned
 * bigint mantissa:
 *
 *   1. Any NaN              -> false.
 *   2. Both Inf              -> sign(u) === sign(v).
 *   3. Both Zero             -> true (zeros compare equal regardless of sign).
 *   4. Only one singular     -> false.
 *   5. Signs differ          -> false.
 *   6. Exponents differ      -> false.
 *   7. Otherwise: compare the top `k = min(n_bits, u.prec, v.prec)` bits
 *      of each MSB-aligned mantissa, by right-shifting each by
 *      `(its_prec - k)`.
 *
 * C-compat trap (mpfr/src/eq.c L113): when `n_bits == 0` the C body's
 * unsigned arithmetic `1 + (n_bits - 1) / GMP_NUMB_BITS` underflows; the
 * size is NOT reduced and the comparison falls through to the top
 * GMP_NUMB_BITS (64) of the most-significant limb. We mirror that
 * semantic by promoting `n_bits=0n` to `64n` before the alignment step.
 *
 * Ref: mpfr/src/eq.c L27-L140 -- C reference body.
 * Ref: CLAUDE.md "NaN != NaN" callout -- any NaN returns false.
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Test whether `u` and `v` agree on their top `n_bits` bits.
 *
 * @mpfrName mpfr_eq
 *
 * @param u       Left operand.
 * @param v       Right operand.
 * @param n_bits  Number of leading bits to compare. `0n` is the C
 *                underflow case (compares the top 64 bits).
 * @returns       `true` iff the leading bits match per the steps above.
 */
export function mpfr_eq(u: MPFR, v: MPFR, n_bits: bigint): boolean {
  if (typeof n_bits !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_eq: n_bits must be bigint, got ${typeof n_bits}`);
  }
  // (1) NaN poisons the comparison on either side.
  if (u.kind === 'nan' || v.kind === 'nan') return false;
  // (2)-(4) Singular dispatch.
  if (u.kind === 'inf' || v.kind === 'inf') {
    if (u.kind === 'inf' && v.kind === 'inf') return u.sign === v.sign;
    return false;
  }
  if (u.kind === 'zero' || v.kind === 'zero') {
    if (u.kind === 'zero' && v.kind === 'zero') return true;
    return false;
  }
  // Both normal from here on.
  // (5) Sign mismatch.
  if (u.sign !== v.sign) return false;
  // (6) Exponent mismatch.
  if (u.exp !== v.exp) return false;
  // (7) Compare the top k bits. n_bits=0 reproduces the C unsigned-
  // underflow path that compares the topmost 64 bits.
  const effective = n_bits === 0n ? 64n : n_bits;
  // k = min(effective, u.prec, v.prec).
  let k = effective;
  if (u.prec < k) k = u.prec;
  if (v.prec < k) k = v.prec;
  return (u.mant >> (u.prec - k)) === (v.mant >> (v.prec - k));
}
