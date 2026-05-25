/**
 * reference_ports/correct/mpfr_eq.ts -- mutation-prove reference for mpfr_eq.
 *
 * Algorithm (mpfr/src/eq.c L27-L140), simplified by the MSB-aligned
 * bigint mantissa:
 *
 *   1. NaN: any NaN -> 0 (false).
 *   2. Both inf: sign(u) == sign(v).
 *   3. Both zero: true (zeros equal regardless of sign for this op).
 *   4. Only one singular: false.
 *   5. Signs differ: false.
 *   6. Exponents differ: false.
 *   7. Otherwise: align mantissas to a common high-bit count and
 *      compare the top n_bits.
 *
 * Two normals with the same exp and sign: the value depends on the top
 * n_bits of mant after shifting to a common reference frame. Since each
 * MSB-aligned mantissa has its leading bit at position `prec - 1`, we
 * compare the top n_bits by shifting both mantissas right by
 * (prec - n_bits).
 *
 * For different precs, we align both to the SMALLER prec by right-
 * shifting the higher-prec mantissa.
 *
 * Ref: mpfr/src/eq.c L27-L140 -- C reference.
 * Ref: src/core.ts L113-L135 -- MPFR shape.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_eq(u: MPFR, v: MPFR, n_bits: bigint): boolean {
  if (typeof n_bits !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_eq: n_bits must be bigint, got ${typeof n_bits}`);
  }
  // Step 1: NaN handling.
  if (u.kind === 'nan' || v.kind === 'nan') return false;
  // Step 2-4: singular handling.
  if (u.kind === 'inf' || v.kind === 'inf') {
    if (u.kind === 'inf' && v.kind === 'inf') return u.sign === v.sign;
    return false;
  }
  if (u.kind === 'zero' || v.kind === 'zero') {
    if (u.kind === 'zero' && v.kind === 'zero') return true;
    return false;
  }
  // Both normal.
  // Step 5: sign.
  if (u.sign !== v.sign) return false;
  // Step 6: exponent.
  if (u.exp !== v.exp) return false;
  // Step 7: compare top n_bits.
  // C-compat trap (mpfr/src/eq.c L113): `n_bits=0` underflows the unsigned
  // arithmetic `1 + (n_bits - 1) / GMP_NUMB_BITS`, so size is NOT reduced
  // and the final compare runs on the most-significant limb. Effectively
  // n_bits=0 compares the top GMP_NUMB_BITS=64 bits, returning false when
  // the topmost 64 bits differ. We mirror that semantic.
  const effective_n_bits = n_bits === 0n ? 64n : n_bits;
  // Align u.mant and v.mant to the same number of significant bits.
  // Each is MSB-aligned to its own prec: u.mant in [2^(u.prec-1), 2^u.prec),
  // and similarly for v.mant.
  // To compare the top k = min(n_bits, u.prec, v.prec) bits, shift each
  // right by (its_prec - k).
  const k = effective_n_bits < u.prec
    ? (effective_n_bits < v.prec ? effective_n_bits : v.prec)
    : (u.prec < v.prec ? u.prec : v.prec);
  const uShift = u.prec - k;
  const vShift = v.prec - k;
  return (u.mant >> uShift) === (v.mant >> vShift);
}
