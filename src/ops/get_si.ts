/**
 * ops/get_si.ts — pure-TS port of MPFR's `mpfr_get_si`.
 *
 * Convert an {@link MPFR} value to a signed 64-bit integer, rounded per
 * the rounding mode. Inverse-shaped to `mpfr_set_si`.
 *
 * C signature
 * -----------
 *
 *   long mpfr_get_si(mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Behaviour (mpfr/src/get_si.c L25–L91):
 *     - NaN              → 0, sets ERANGE flag
 *     - +Inf / overflows → LONG_MAX, sets ERANGE flag
 *     - -Inf / underflow → LONG_MIN, sets ERANGE flag
 *     - ±0               → 0 (exact)
 *     - finite in range  → rounded integer per rnd
 *
 *   The C algorithm:
 *     1. `mpfr_fits_slong_p(f, rnd)`: tests whether the rounded value
 *        fits in `long`. If not, set ERANGE and saturate.
 *     2. If `MPFR_IS_ZERO`, return 0.
 *     3. Compute `prec = bitLength(LONG_MAX) == 63`, allocate a scratch
 *        `mpfr_t x` at that precision, call `mpfr_rint(x, f, rnd)` to
 *        round to the nearest integer per `rnd`.
 *     4. Read out the rounded integer by extracting limb bits — for
 *        `MPFR_LONG_WITHIN_LIMB`, just `MPFR_MANT(x)[n-1] >> (GMP_NUMB_BITS
 *        - exp)`.
 *     5. Apply the sign; for negatives, take 2's complement (`-(long)u`)
 *        unless `u == |LONG_MIN|`, in which case the special-case branch
 *        returns `LONG_MIN` directly (avoids signed-int UB from `-LONG_MIN`).
 *
 * TS divergence
 * -------------
 *
 *   mpfr_get_si(x: MPFR, rnd: RoundingMode): bigint;   // throws on ERANGE
 *
 * Per the pattern set in mpfr_cmp / mpfr_get_d (the get-family): we
 * THROW `MPFRError('EPREC', ...)` instead of returning a saturated
 * sentinel value, because:
 *
 *   - The locked schema (src/core.ts) has no ERANGE flag; mirroring
 *     C's "side-channel global flag" pattern requires either a mutable
 *     module-level state (incompatible with Law 3's immutable surface)
 *     or returning `{value, flag}` (a new schema shape inconsistent
 *     with the rest of the get-family).
 *   - Silent saturation hides bugs. A caller that passed Infinity by
 *     mistake gets LONG_MAX and continues; a thrown error stops the
 *     computation at the trust boundary.
 *
 * The discriminant is `EPREC` for the same reason set_si uses it for
 * range errors: the locked enum has only EPREC / EROUND / EDOMAIN, and
 * EPREC is the closest fit for "bad input argument" / "result outside
 * the target type's range".
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `rnd`; structurally validate `x`.
 *   2. Specials:
 *        NaN  → throw MPFRError('EPREC', ...)
 *        ±Inf → throw MPFRError('EPREC', ...)
 *        ±0   → return 0n
 *   3. Normal: round to an integer per `rnd`, then range-check against
 *      `[LONG_MIN, LONG_MAX]`. Out-of-range throws.
 *
 *      The "round to integer" step is the load-bearing piece. The value
 *      is `sign * mant * 2^(exp - prec)`. Three sub-cases on `(exp -
 *      prec)`:
 *
 *        - `exp - prec >= 0`: the value is an exact integer with no
 *          fractional bits. `intPart = mant << (exp - prec)`. No
 *          rounding occurs; ternary at the round step is 0.
 *
 *        - `exp <= 0`: `|value| < 1`. The integer truncation is 0;
 *          rounding direction depends on the rnd mode and sign. We
 *          treat the value as if dropping ALL bits — which `roundMantissa`
 *          can't do directly because the substrate requires
 *          `outPrec >= 1`. Special-case here.
 *
 *        - `0 < exp - prec`: covered by the first sub-case (exact int).
 *
 *        - `0 < exp < prec` (i.e. `exp >= 1` and `exp - prec < 0`):
 *          the value has both integer and fractional parts. We delegate
 *          to `roundMantissa` with `outPrec = exp` — i.e. "round the
 *          mantissa to a width such that the LSB of the rounded
 *          mantissa is the units position". The result mantissa,
 *          interpreted at that width, is exactly the rounded integer
 *          (possibly with a carry-out bumping the exp).
 *
 *   4. Apply sign and bounds check. Return as a JS bigint.
 *
 * Why bigint and not number
 * -------------------------
 *
 * `Number.MAX_SAFE_INTEGER` is `2^53 - 1`, well below `LONG_MAX`. A
 * `get_si` that returned `number` would silently round to the nearest
 * double for values above 2^53 — defeating the whole point of the
 * conversion. The schema-wide convention (mpfr_set_si takes bigint,
 * comparison ops use bigint for limb-shaped values) is bigint for any
 * integer-typed boundary value that could exceed 2^53.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/get_si.c L25–L91 — C reference.
 *   - mpfr/src/fits_s.h — the underlying mpfr_fits_slong_p logic.
 *   - mpfr/src/rint.c — the rounding step.
 *   - src/internal/mpfr/round_raw.ts — shared rounding substrate.
 *   - src/core.ts — locked schema.
 *   - src/ops/get_d.ts — sibling conversion-out op (Inf and overflow
 *     saturate there since IEEE 754 carries Infinity; the int target
 *     has no analog, hence the throw divergence).
 *   - CLAUDE.md "Hallucination-risk callouts" — NaN ≠ NaN (we throw
 *     rather than return 0 with a hidden flag); rounding-mode count
 *     is FIVE.
 */

import type { MPFR, RoundingMode, Sign } from '../core.ts';
import { MPFRError, validate } from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

const LONG_MIN_VAL: bigint = -(1n << 63n);
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

/**
 * Validate the rounding mode argument. The {@link validate} from
 * src/core.ts handles the MPFR value's structural invariants.
 */
function validateRnd(rnd: RoundingMode): void {
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Round a non-singular MPFR value to an integer per `rnd`, returning the
 * absolute integer magnitude as a non-negative bigint. The caller
 * re-applies the sign.
 *
 * Pre-condition: `x.kind === 'normal'` and `x` passes `validate()`.
 *
 * Strategy (in the schema's value formula `sign * mant * 2^(exp - prec)`):
 *
 *   - If `exp - prec >= 0` (i.e. `exp >= prec`): the value is already an
 *     integer. Magnitude = `mant << (exp - prec)`. No rounding.
 *
 *   - If `exp <= 0` (`|value| < 1`): the integer part is 0. Rounding
 *     direction follows `rnd` and the value's sign. Return 0n or 1n.
 *
 *   - If `0 < exp < prec`: the value has both integer (top `exp` bits)
 *     and fractional (bottom `prec - exp` bits) parts. Use
 *     `roundMantissa(mant, prec, exp, exp, sign, rnd)` to round the
 *     mantissa down to `exp` bits (= the integer-part width). The
 *     substrate's carry-out handling bumps the exponent on overflow
 *     (e.g. rounding `0.111...1` upward in the fraction promotes the
 *     integer part by 1).
 */
function roundToInteger(x: MPFR, sign: Sign, rnd: RoundingMode): bigint {
  const exp = x.exp;
  const prec = x.prec;
  const mant = x.mant;

  if (exp >= prec) {
    // Exact integer: mantissa shifted left by (exp - prec).
    return mant << (exp - prec);
  }

  if (exp <= 0n) {
    // |value| < 1. The "all dropped" case the substrate doesn't cover.
    // Decide rounding direction explicitly:
    //
    //   - RNDZ: 0 (truncation toward zero)
    //   - RNDA: 1 (away from zero)
    //   - RNDD: 0 if positive, 1 if negative (toward -∞)
    //   - RNDU: 1 if positive, 0 if negative (toward +∞)
    //   - RNDN: half-ulp boundary is at |value| = 1/2, i.e. when
    //          exp == 0 AND mant == 2^(prec-1) (the value is exactly ±0.5).
    //          mant > 2^(prec-1) only when exp > 0, so for exp <= 0 the
    //          half is at the boundary `exp == 0` exactly.
    //
    // Detail for RNDN: |value| in [2^(exp-1), 2^exp). For exp == 0,
    // |value| ∈ [1/2, 1). The mant > 2^(prec-1) case puts |value|
    // strictly above 1/2; mant == 2^(prec-1) (the MSB-only mantissa)
    // gives |value| == 1/2 exactly — the tie. For exp < 0,
    // |value| < 1/2, so always round down.
    switch (rnd) {
      case 'RNDZ':
        return 0n;
      case 'RNDA':
        return 1n;
      case 'RNDD':
        return sign === -1 ? 1n : 0n;
      case 'RNDU':
        return sign === 1 ? 1n : 0n;
      case 'RNDN': {
        if (exp < 0n) {
          // |value| < 1/2 — always round to 0.
          return 0n;
        }
        // exp === 0n: |value| ∈ [1/2, 1).
        const halfMant = 1n << (prec - 1n);
        if (mant > halfMant) {
          // Strictly above 1/2 — round to 1.
          return 1n;
        }
        // Exactly 1/2 (the tie). Round to even: the LSB of the integer
        // representation must be even. Since the integer target is 0 or
        // 1, even is 0. Round to 0.
        return 0n;
      }
      default: {
        const _exhaustive: never = rnd;
        void _exhaustive;
        throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
      }
    }
  }

  // 0 < exp < prec: round the prec-bit mantissa down to exp bits via the
  // substrate. The result's `mant` (at outPrec=exp) is the rounded
  // integer magnitude; the result's `exp` is `exp` (no carry) or
  // `exp + 1` (carry-out bumped the integer past 2^exp, e.g. 7.9 → 8
  // under RNDA).
  //
  // Carry-out case: `roundMantissa` renormalises to mant=2^(outPrec-1)
  // and exp+1. The numeric integer magnitude in that case is `2^outPrec`
  // (the renormalised mantissa shifted up by 1 — equivalently the
  // un-renormalised pre-bump mant). We reconstruct that by checking the
  // returned exp against the input exp.
  const rounded = roundMantissa(mant, prec, exp, exp, sign, rnd);
  if (rounded.exp === exp) {
    // No carry. rounded.mant is the rounded integer magnitude.
    return rounded.mant;
  }
  // Carry-out: the rounded value bumped past 2^exp. The integer magnitude
  // is 2^exp (i.e. rounded.mant shifted up by 1 since it was renormalised
  // to half).
  //
  // Sanity: substrate returns mant = 2^(outPrec-1) = 2^(exp-1) and
  // exp+1 in the carry case. The mathematical integer is 2^exp.
  return 1n << exp;
}

/**
 * Convert an MPFR value to a signed 64-bit integer, rounded per `rnd`.
 *
 * @mpfrName mpfr_get_si
 *
 * @param x    the MPFR value. Must pass {@link validate}.
 * @param rnd  one of the five rounding modes.
 *
 * @returns    the rounded integer as a `bigint` in `[LONG_MIN, LONG_MAX]`.
 *
 * @throws {MPFRError}
 *   - `EROUND` on unknown rounding mode.
 *   - `EPREC` if `x` is NaN, ±Inf, or rounds to a value outside the
 *     signed int64 range. (C returns LONG_MIN / LONG_MAX with the
 *     ERANGE flag; the TS surface throws — see the module docstring
 *     "TS divergence" section.)
 *
 * @example
 *   mpfr_get_si(setSi(42n, 53n, 'RNDN').value, 'RNDN');     //  42n
 *   mpfr_get_si(setSi(-42n, 53n, 'RNDN').value, 'RNDN');    // -42n
 *   mpfr_get_si(setD(3.7, 53n, 'RNDN').value, 'RNDZ');      //   3n (truncate)
 *   mpfr_get_si(setD(3.7, 53n, 'RNDN').value, 'RNDA');      //   4n (away from zero)
 */
export function mpfr_get_si(x: MPFR, rnd: RoundingMode): bigint {
  validateRnd(rnd);
  validate(x);

  // --- Specials ------------------------------------------------------------
  if (x.kind === 'nan') {
    throw new MPFRError(
      'EPREC',
      'mpfr_get_si: NaN cannot be converted to a signed integer',
    );
  }
  if (x.kind === 'inf') {
    throw new MPFRError(
      'EPREC',
      `mpfr_get_si: ${x.sign === 1 ? '+Inf' : '-Inf'} cannot be converted to a signed integer`,
    );
  }
  if (x.kind === 'zero') {
    // ±0 → 0n. Signed zero collapses on the integer side (an int has no
    // -0 representation).
    return 0n;
  }

  // --- Normal: round and bounds-check --------------------------------------
  const sign = x.sign;
  const absInt = roundToInteger(x, sign, rnd);
  // Apply sign and bounds-check.
  //
  // Negative side: the smallest representable signed int64 is LONG_MIN =
  // -(2^63), whose absolute value 2^63 exceeds LONG_MAX = 2^63 - 1.
  // So `absInt === 2^63` is in range *iff* the sign is negative — that
  // value represents LONG_MIN exactly.
  if (sign === 1) {
    if (absInt > LONG_MAX_VAL) {
      throw new MPFRError(
        'EPREC',
        `mpfr_get_si: value rounds to ${absInt}, exceeds LONG_MAX=${LONG_MAX_VAL}`,
      );
    }
    return absInt;
  }
  // sign === -1
  const absLongMin = 1n << 63n; // |LONG_MIN| = 2^63
  if (absInt > absLongMin) {
    throw new MPFRError(
      'EPREC',
      `mpfr_get_si: value rounds to -${absInt}, below LONG_MIN=${LONG_MIN_VAL}`,
    );
  }
  if (absInt === absLongMin) {
    // Exactly LONG_MIN. Return the literal to avoid bigint sign-overflow
    // worry (and to match the C special-case at mpfr/src/get_si.c L83
    // `u <= LONG_MAX ? -(long)u : LONG_MIN`).
    return LONG_MIN_VAL;
  }
  return -absInt;
}
