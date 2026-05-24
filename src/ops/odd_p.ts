/**
 * ops/odd_p.ts — pure-TS port of MPFR's `mpfr_odd_p`.
 *
 * Surface-class predicate. Returns `true` iff the input is an exact odd
 * integer. Unlike the C function (which asserts non-singular input via
 * `MPFR_ASSERTD`), the TS port returns `false` for all singular inputs
 * (NaN, Inf, zero) — none of those are odd integers, and defensive
 * graceful handling beats an assertion crash in a library context.
 *
 * C signature:
 *
 *   int mpfr_odd_p(mpfr_srcptr y)   // returns 1 (odd) or 0 (not odd)
 *
 * TS signature:
 *
 *   mpfr_odd_p(x: MPFR): boolean
 *
 * Algorithm (from C source, directly transcribed to BigInt arithmetic)
 * --------------------------------------------------------------------
 *
 * In the TS value model, a normal MPFR value is:
 *
 *   value = sign * mant * 2^(exp - prec)
 *
 * where `mant` is a `prec`-bit unsigned integer with the MSB set (bit
 * `prec-1` is always 1).
 *
 * Step 1: if x is not `kind === 'normal'`, return false.
 *
 * Step 2: if exp <= 0, then |x| < 1 (since mant >= 2^(prec-1) and
 *   the scale factor is 2^(exp-prec) <= 2^(-prec) < 2^(-prec+1)); not
 *   an integer at all → false.
 *   Ref: mpfr/src/odd_p.c L605-L606.
 *
 * Step 3: if exp > prec, then x = sign * mant * 2^(exp-prec) where
 *   (exp-prec) >= 1, so x is a non-zero even multiple of 2 → false.
 *   Ref: mpfr/src/odd_p.c L608-L610.
 *
 * Step 4: 0 < exp <= prec.
 *   The value layout in the BigInt mantissa (MSB at bit prec-1):
 *
 *     bit index (from LSB): prec-1 ... (prec-exp) ... 1  0
 *                           [integer part ]        [frac ]
 *
 *   The integer part occupies bits [prec-1 .. prec-exp] (i.e. `exp` bits).
 *   The unit bit (the least significant integer bit) is at position
 *   `unitPos = prec - exp`.
 *
 *   For x to be an odd integer:
 *   (a) the unit bit must be 1: (mant >> unitPos) & 1n === 1n
 *   (b) all fractional bits (positions 0 .. unitPos-1) must be zero:
 *       mant & ((1n << unitPos) - 1n) === 0n
 *
 *   Both conditions are equivalent to: mant & ((1n << (unitPos + 1n)) - 1n)
 *   having exactly bit unitPos set — i.e. the low (unitPos+1) bits of mant
 *   equal exactly (1n << unitPos).
 *
 *   Ref: mpfr/src/odd_p.c L612-L636 — the C implementation checks this
 *   via limb-level bit manipulation (MPFR_LIMB_LSHIFT and zero-tail
 *   limb scan); in TS we operate directly on the BigInt mantissa.
 *
 * Refs
 * ----
 *   - mpfr/src/odd_p.c L25-L71 — the C reference body.
 *   - src/core.ts L113-L135 — MPFR TS value model (sign * mant * 2^(exp-prec)).
 *   - CLAUDE.md "Hallucination-risk callouts: mpfr_prec_t is in bits".
 */

import type { MPFR } from "../core.ts";

/**
 * Returns `true` iff `x` is an exact odd integer.
 *
 * Singular inputs (NaN, Inf, zero) return `false`. Normal values are
 * tested via BigInt bit-arithmetic on the mantissa.
 *
 * @mpfrName mpfr_odd_p
 *
 * @param x The MPFR value to test.
 * @returns `true` if `x` is an odd integer, `false` otherwise.
 */
export function mpfr_odd_p(x: MPFR): boolean {
  // Step 1: singular inputs are never odd integers.
  // Ref: mpfr/src/odd_p.c L601 — MPFR_ASSERTD(!MPFR_IS_SINGULAR(y))
  // The C function asserts non-singular; TS port relaxes to return false.
  if (x.kind !== 'normal') {
    return false;
  }

  const { exp, prec, mant } = x;

  // Step 2: exp <= 0 means |x| < 1 (non-integer) → false.
  // Ref: mpfr/src/odd_p.c L605-L606.
  if (exp <= 0n) {
    return false;
  }

  // Step 3: exp > prec means x is divisible by 2^(exp-prec) with
  // exp-prec >= 1 → x is even → false.
  // Ref: mpfr/src/odd_p.c L608-L610.
  if (exp > prec) {
    return false;
  }

  // Step 4: 0 < exp <= prec.
  // The unit bit (position of the '1' digit in x's binary representation)
  // is at bit index `unitPos = prec - exp` within `mant` (0 = LSB).
  //
  // Odd integer iff:
  //   (a) unit bit is set:          (mant >> unitPos) & 1n === 1n
  //   (b) all bits below unitPos are zero: mant & ((1n << unitPos) - 1n) === 0n
  //
  // Ref: mpfr/src/odd_p.c L619-L636.
  const unitPos = prec - exp;  // bigint, >= 0

  // Check bit (a): the unit bit must be 1.
  if (((mant >> unitPos) & 1n) !== 1n) {
    return false;
  }

  // Check bit (b): all fractional bits below unitPos must be zero.
  // If unitPos === 0n there are no fractional bits, so the mask is 0n and
  // this check trivially passes.
  if (unitPos > 0n) {
    const fracMask = (1n << unitPos) - 1n;
    if ((mant & fracMask) !== 0n) {
      return false;
    }
  }

  return true;
}
