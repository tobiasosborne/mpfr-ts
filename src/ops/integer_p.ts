/**
 * mpfr_integer_p — pure-TS port of MPFR's `mpfr_integer_p`.
 *
 * Surface-class (arithmetic) function. Returns `true` iff the MPFR value `x`
 * represents an exact integer. No rounding mode is involved; this is a pure
 * predicate. No `Result` wrapper — the C function returns `int` (0 or 1);
 * the idiomatic TS port returns `boolean`.
 *
 * C signature:
 *   int mpfr_integer_p(mpfr_srcptr x)
 *
 * Ref: mpfr/src/isinteger.c L25-L58 — canonical C implementation.
 *
 * Algorithm (mirroring the C source exactly):
 *
 * 1. Singular dispatch (C: `MPFR_IS_SINGULAR`):
 *    - Zero (`kind === 'zero'`): IS an integer → return true.
 *    - NaN / Inf: are NOT integers → return false.
 *    Ref: mpfr/src/isinteger.c L33-L34 — `return (MPFR_IS_ZERO(x))`.
 *
 * 2. If `exp <= 0n`: the value is `±fraction` with |x| in [2^(exp-1), 2^exp)
 *    and exp ≤ 0, so |x| < 1 (and x ≠ 0, handled above) → NOT an integer.
 *    Ref: mpfr/src/isinteger.c L37-L38 — `if (expo <= 0) return 0`.
 *
 * 3. If `exp >= prec`: the value = sign * mant * 2^(exp-prec). Since
 *    mant has exactly `prec` bits and we're scaling up by 2^(exp-prec)
 *    where exp ≥ prec → all bits are above the binary point → IS an integer.
 *    Ref: mpfr/src/isinteger.c L41-L42 — `if (expo >= prec) return 1`.
 *
 * 4. Otherwise `0 < exp < prec`: the mantissa has `(prec - exp)` fractional
 *    bits (the low `prec - exp` bits of `mant`). The value is an integer iff
 *    all those bits are zero.
 *    Ref: mpfr/src/isinteger.c L44-L57 — the limb-walking loop checks each
 *      limb that contains fractional bits for zero. In our BigInt representation,
 *      this collapses to a single bitmask check.
 *
 * TS value model note:
 *   The value of a `normal` MPFR is `sign * mant * 2^(exp - prec)`.
 *   The binary point sits `prec - exp` bits from the right of `mant`.
 *   The fractional bits are the low `(prec - exp)` bits of `mant`.
 *   Ref: src/core.ts L113-L135 — value formula: `sign * mant * 2^(exp - prec)`.
 */

import type { MPFR } from "../core.ts";

/**
 * Returns `true` iff `x` represents an exact integer value.
 *
 * - Zero → true (zero is an integer).
 * - NaN, Inf → false (not integers).
 * - Normal value: true iff the mantissa has no fractional bits set.
 *
 * @mpfrName mpfr_integer_p
 * @param x An MPFR value.
 * @returns boolean
 *
 * Ref: mpfr/src/isinteger.c L25-L58 — full C body.
 */
export function mpfr_integer_p(x: MPFR): boolean {
  // Step 1: Singular dispatch.
  // Ref: mpfr/src/isinteger.c L33-L34 — `MPFR_IS_SINGULAR` branch.
  if (x.kind !== 'normal') {
    // zero → true; nan, inf → false
    return x.kind === 'zero';
  }

  // x is normal (finite, nonzero). The value is sign * mant * 2^(exp - prec).
  const exp = x.exp;
  const prec = x.prec;

  // Step 2: exp <= 0 → |x| < 1 (since x ≠ 0) → not an integer.
  // Ref: mpfr/src/isinteger.c L37-L38.
  if (exp <= 0n) {
    return false;
  }

  // Step 3: exp >= prec → no fractional bits → integer.
  // Ref: mpfr/src/isinteger.c L41-L42.
  if (exp >= prec) {
    return true;
  }

  // Step 4: 0 < exp < prec → check that the low (prec - exp) bits of mant are all zero.
  // The fractional part lives in bits [0, prec - exp - 1] of mant.
  // Ref: mpfr/src/isinteger.c L44-L57 — limb loop checking fractional bits.
  const fracBits = prec - exp; // number of fractional bits (> 0)
  const fracMask = (1n << fracBits) - 1n;
  return (x.mant & fracMask) === 0n;
}
