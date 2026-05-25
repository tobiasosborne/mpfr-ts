/**
 * ops/check.ts -- pure-TS port of MPFR's `mpfr_check`.
 *
 * Returns `true` iff `x` is a structurally well-formed MPFR value
 * against the locked schema. The C reference (mpfr/src/check.c L32-L80)
 * walks a battery of memory-layout invariants (sign field, prec range,
 * mantissa pointer non-null, allocation size matches precision, MSB
 * bit set, trailing-bit padding zero, exponent in range). The TS
 * analogue is `validate()` from `core.ts`, which enforces the same
 * conceptual invariants on the bigint representation:
 *
 *   - kind in {normal, zero, inf, nan}
 *   - sign in {1, -1}
 *   - prec a bigint in [PREC_MIN, PREC_MAX] (or 0n for NaN)
 *   - For normal: 2^(prec-1) <= mant < 2^prec  (MSB-aligned)
 *   - For singular: exp=0n, mant=0n; NaN additionally sign=1, prec=0n
 *
 * `mpfr_check(x)` reduces to `validate(x)` swallowed into a boolean.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/check.c L32-L80 -- C reference body.
 *   - src/core.ts L354-L437 -- validate() -- the conceptual TS analogue.
 *   - src/core.ts L113-L135 -- MPFR shape invariants.
 */

import type { MPFR } from '../core.ts';
import { validate } from '../core.ts';

/**
 * Structural well-formedness predicate for an MPFR value.
 *
 * @mpfrName mpfr_check
 *
 * @param x  An MPFR value (typically one produced by another op or
 *           decoded from the wire).
 * @returns  `true` iff `validate(x)` passes; `false` otherwise.
 *
 * @example
 *   mpfr_check(posZero(53n));  // true
 *   mpfr_check(NAN_VALUE);     // true
 */
export function mpfr_check(x: MPFR): boolean {
  try {
    validate(x);
    return true;
  } catch {
    return false;
  }
}
