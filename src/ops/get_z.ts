/**
 * ops/get_z.ts — pure-TS port of MPFR's `mpfr_get_z`.
 *
 * Round an {@link MPFR} value to the nearest integer per `rnd` and
 * return it as an arbitrary-precision integer (`bigint`). Inverse of
 * mpfr_set_z.
 *
 * C signature
 * -----------
 *
 *   int mpfr_get_z(mpz_ptr z, mpfr_srcptr f, mpfr_rnd_t rnd);
 *
 *   Mutates `z`. Returns the ternary `(rounded - exact)` as the int
 *   function result. Behaviour on singulars (mpfr/src/get_z.c L33–L42):
 *     - NaN / Inf  → sets z to 0, sets ERANGE flag, returns 0.
 *     - ±0         → sets z to 0, no flag, returns 0.
 *
 *   Normal-value path (mpfr/src/get_z.c L44–L65):
 *     1. Allocate scratch `r` at precision `max(MPFR_PREC_MIN, MPFR_GET_EXP(f))`.
 *     2. inex = mpfr_rint(r, f, rnd) — rounds f to an integer per rnd.
 *     3. e = mpfr_get_z_2exp(z, r); apply 2^e shift via mpz_mul_2exp or
 *        mpz_fdiv_q_2exp depending on sign of e.
 *     4. Return inex.
 *
 * TS divergence
 * -------------
 *
 *   mpfr_get_z(x: MPFR, rnd: RoundingMode): bigint;
 *
 * Following the same convention as `mpfr_get_si` (src/ops/get_si.ts):
 *
 *   - THROWS `MPFRError('EPREC', ...)` on NaN / ±Inf instead of
 *     returning 0 with a hidden ERANGE flag. The locked schema has no
 *     flag surface; silent saturation hides bugs at the trust boundary.
 *   - Returns ONLY the integer value, not the ternary pair. The use
 *     case for get_z is "I want an integer out"; callers who need the
 *     ternary can compute it themselves by re-rounding (or by calling
 *     `mpfr_rint(x, rnd)` once that's ported and then comparing).
 *     Documenting the divergence in spec.json and the JSDoc here.
 *   - Returns `bigint` rather than `number` — get_z is the
 *     arbitrary-precision sibling of get_si, and `Number.MAX_SAFE_INTEGER`
 *     is far below what get_z can produce.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `rnd`; structurally validate `x`.
 *   2. NaN → throw. ±Inf → throw. ±0 → return 0n.
 *   3. Normal: round to integer via the schema's value formula
 *      `sign * mant * 2^(exp - prec)`. Three sub-cases on `exp - prec`:
 *
 *      - `exp >= prec`: value is already an integer. Magnitude is
 *        `mant << (exp - prec)`. No rounding occurs.
 *
 *      - `exp <= 0`: `|value| < 1`. Integer truncation is 0; round
 *        direction depends on rnd, sign, and (for RNDN) whether
 *        |value| == 1/2 exactly. Return 0n or sign * 1n.
 *
 *      - `0 < exp < prec`: value has both integer and fractional
 *        parts. Delegate to roundMantissa with outPrec = exp, so the
 *        rounded mantissa (interpreted at width exp) IS the rounded
 *        integer magnitude. Carry-out promotes the magnitude to 2^exp.
 *
 *   4. Apply sign and return.
 *
 * No range check — `bigint` can represent any rounded integer the
 * value can produce, so there's no overflow case to throw on (unlike
 * get_si's int64 cap).
 *
 * Why classed `misc` (1s budget)
 * ------------------------------
 *
 * For a very large input (e.g. `mpfr_set_z(huge_bigint, 1000, 'RNDN').value`
 * with exp ≈ huge_bigint.bitLength), the integer result is itself
 * thousands of bits — the `mant << (exp - prec)` shift is the limiting
 * step, dominated by bigint allocation. 1s headroom comfortably covers
 * inputs of millions of bits.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/get_z.c L25–L70 — C reference.
 *   - mpfr/src/rint.c — the round-to-integer primitive (we inline its
 *     two main branches in roundToInteger below).
 *   - mpfr/src/get_z_2exp.c L50–L95 — the 2^e shift step (we apply it
 *     as the final `mant << (exp - prec)` shift in the integer branch).
 *   - src/internal/mpfr/round_raw.ts — substrate.
 *   - src/ops/get_si.ts — sibling get-family op for the int64-bounded
 *     case; shares the round-then-extract algorithm.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — NaN ≠ NaN (we throw
 *     rather than return a flagged 0); rounding-mode count = FIVE.
 */

import type { MPFR, RoundingMode, Sign } from '../core.ts';
import { MPFRError, validate } from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

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
 * Round a non-singular MPFR value to an integer per `rnd`, returning
 * the absolute integer magnitude as a non-negative bigint. The caller
 * re-applies the sign.
 *
 * Pre-condition: `x.kind === 'normal'` and `x` passes `validate()`.
 *
 * Shape exactly mirrors `roundToInteger` in src/ops/get_si.ts; the only
 * difference is that get_z has no upper bound (no LONG_MAX cap to
 * watch). See that file for the algorithm-level documentation; here we
 * just re-implement to avoid an internal dependency.
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
    // |value| < 1 — the integer part is 0; rounding direction follows
    // rnd, sign, and (for RNDN) the half-ulp boundary. Same logic as
    // get_si.ts:
    //
    //   RNDZ → 0
    //   RNDA → 1
    //   RNDD → 0 if positive, 1 if negative (toward -∞)
    //   RNDU → 1 if positive, 0 if negative (toward +∞)
    //   RNDN → |value| == 1/2 exactly iff `exp == 0n && mant == 2^(prec-1)`;
    //          tie → round to even (returns 0 since target is integer 0/1).
    //          |value| > 1/2 (exp == 0, mant > 2^(prec-1)) → 1.
    //          |value| < 1/2 (exp < 0) → 0.
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
        if (exp < 0n) return 0n;
        const halfMant = 1n << (prec - 1n);
        if (mant > halfMant) return 1n;
        // Exactly 1/2 — tie-to-even: integer 0 is even.
        return 0n;
      }
      default: {
        const _exhaustive: never = rnd;
        void _exhaustive;
        throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
      }
    }
  }

  // 0 < exp < prec: value has integer + fractional parts. Round the
  // prec-bit mantissa down to exp bits via the substrate. The result's
  // `mant` at outPrec=exp is the rounded integer magnitude; if the
  // substrate carried out (exp += 1), the magnitude is 2^exp.
  const rounded = roundMantissa(mant, prec, exp, exp, sign, rnd);
  if (rounded.exp === exp) {
    return rounded.mant;
  }
  // Carry-out: the rounded value bumped past 2^exp. The integer
  // magnitude is exactly 2^exp.
  return 1n << exp;
}

/**
 * Round an MPFR value to an integer per `rnd` and return it as a bigint.
 *
 * @mpfrName mpfr_get_z
 *
 * @param x    the MPFR value. Must pass {@link validate}.
 * @param rnd  one of the five rounding modes.
 *
 * @returns    the rounded integer as a `bigint`. Has no upper bound:
 *             very large inputs (e.g. a value with huge `exp`) yield
 *             correspondingly large integers.
 *
 * @throws {MPFRError}
 *   - `EROUND` on unknown rounding mode.
 *   - `EPREC` if `x` is NaN or ±Inf. (C returns 0 with the ERANGE flag;
 *     the TS surface throws — same divergence as get_si.)
 *
 * @example
 *   mpfr_get_z(mpfr_set_z(42n, 53n, 'RNDN').value, 'RNDN');           // 42n
 *   mpfr_get_z(mpfr_set_d(3.7, 53n, 'RNDN').value, 'RNDZ');           // 3n
 *   mpfr_get_z(mpfr_set_d(3.7, 53n, 'RNDN').value, 'RNDA');           // 4n
 *   mpfr_get_z(mpfr_set_z(1n << 200n, 200n, 'RNDN').value, 'RNDN');   // 2^200 as bigint
 */
export function mpfr_get_z(x: MPFR, rnd: RoundingMode): bigint {
  validateRnd(rnd);
  validate(x);

  if (x.kind === 'nan') {
    throw new MPFRError(
      'EPREC',
      'mpfr_get_z: NaN cannot be converted to an integer',
    );
  }
  if (x.kind === 'inf') {
    throw new MPFRError(
      'EPREC',
      `mpfr_get_z: ${x.sign === 1 ? '+Inf' : '-Inf'} cannot be converted to an integer`,
    );
  }
  if (x.kind === 'zero') {
    // ±0 → 0n. Signed zero collapses on the integer side (bigint has
    // no -0n).
    return 0n;
  }

  const sign = x.sign;
  const absInt = roundToInteger(x, sign, rnd);
  return sign === 1 ? absInt : -absInt;
}
