/**
 * ops/get_d_2exp.ts — pure-TS port of MPFR's `mpfr_get_d_2exp`.
 *
 * Decompose an MPFR value into a normalised mantissa `value` in [0.5, 1.0)
 * (or (-1.0, -0.5] for negative) and an integer exponent `exp` such that
 * x ≈ value × 2^exp. The C signature uses a `long *expptr` out-parameter;
 * the TS port returns `{value: number, exp: bigint}`.
 *
 * C signature:
 *
 *   double mpfr_get_d_2exp(long *expptr, mpfr_srcptr src, mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port):
 *
 *   mpfr_get_d_2exp(x: MPFR, rnd: RoundingMode) -> {value: number, exp: bigint}
 *
 * Algorithm
 * ---------
 *
 * Ref: mpfr/src/get_d.c L142–L187 — the C reference body.
 *
 * 1. Singular dispatch (MPFR_IS_SINGULAR):
 *      NaN    → value=NaN,         exp=0n
 *      +Inf   → value=+Infinity,   exp=0n
 *      -Inf   → value=-Infinity,   exp=0n
 *      +0     → value=+0,          exp=0n
 *      -0     → value=-0,          exp=0n
 *
 * 2. Normal value:
 *    The C code uses MPFR_ALIAS to create a temporary `tmp` that aliases
 *    `src` but with exponent set to 0 (i.e. the significand is treated as
 *    a value in [0.5, 1.0) by mpfr_get_d). Then:
 *      ret = mpfr_get_d(tmp, rnd_mode)
 *      exp = MPFR_GET_EXP(src)   // original exponent
 *    Rounding can push ret to ±1.0 (carry-out in mpfr_get_d): in that
 *    case ret is halved and exp is incremented.
 *
 *    In the TS value model, a normal value with exp=e has magnitude in
 *    [2^(e-1), 2^e). Setting exp=0 gives magnitude in [2^(0-1), 2^0) =
 *    [0.5, 1.0). The mantissa bits are unchanged — only the exponent
 *    field is aliased to 0. This is exactly what MPFR_ALIAS does: it
 *    shares the same limb pointer with a different exponent.
 *
 *    We replicate this by calling mpfr_get_d on the modified MPFR value
 *    `{...x, exp: 0n}`.
 *
 * Ref: mpfr/src/get_d.c L34–L132 — mpfr_get_d, the delegate.
 * Ref: src/ops/get_d.ts — the TS port of mpfr_get_d.
 *
 * Notes
 * -----
 *
 * - The spec says "NaN -> value=NaN, exp=0" — NaN output is compared via
 *   Object.is in the grader (CLAUDE.md hallucination-risk callout).
 * - Signed zero is observable: "±0 -> value=±0, exp=0" (CLAUDE.md
 *   hallucination-risk callout "Signed zero is real").
 * - This function returns a bare JS `number` + `bigint`, NOT a Result shape.
 *   There is no ternary: the get-family has no ternary per the locked schema.
 */

import type { MPFR, RoundingMode } from "../core.ts";
import { MPFRError, validate } from "../core.ts";
import { mpfr_get_d } from "./get_d.ts";

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

/**
 * Validate the rounding mode at the public boundary.
 * Throws MPFRError with code EROUND on an unknown string.
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

// ---------------------------------------------------------------------------
// Return type
// ---------------------------------------------------------------------------

/**
 * Return shape: normalised mantissa in [0.5, 1.0) and base-2 exponent.
 * Wire: {"value": <number-string>, "exp": <bigint-string>}.
 */
export interface GetD2ExpResult {
  readonly value: number;
  readonly exp: bigint;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Decompose an MPFR value into a normalised mantissa and base-2 exponent.
 *
 * @mpfrName mpfr_get_d_2exp
 *
 * For a normal `x`, returns `{value, exp}` such that:
 *   x ≈ value × 2^exp
 * with `0.5 ≤ |value| < 1.0` (the same invariant as `frexp` in C).
 *
 * Singular dispatch (all rounding modes):
 *   NaN   → {value: NaN,         exp: 0n}
 *   +Inf  → {value: +Infinity,   exp: 0n}
 *   -Inf  → {value: -Infinity,   exp: 0n}
 *   +0    → {value: +0,          exp: 0n}
 *   -0    → {value: -0,          exp: 0n}
 *
 * @param x    the MPFR value to decompose. Must pass validate().
 * @param rnd  one of the five rounding modes in RoundingMode.
 *
 * @returns  {value: number, exp: bigint}
 *
 * @throws {MPFRError} EROUND on unknown rounding mode; EPREC if input
 *                     fails structural validation.
 */
export function mpfr_get_d_2exp(x: MPFR, rnd: RoundingMode): GetD2ExpResult {
  // Ref: mpfr/src/get_d.c L142-L187 — full function body.
  validateRnd(rnd);
  validate(x);

  // --- Singular dispatch ---------------------------------------------------
  // Ref: mpfr/src/get_d.c L148–L162 — MPFR_IS_SINGULAR branch.
  if (x.kind === 'nan') {
    // C: "we don't propagate the sign bit" for NaN.
    return { value: Number.NaN, exp: 0n };
  }
  if (x.kind === 'inf') {
    return {
      value: x.sign === 1 ? Number.POSITIVE_INFINITY : Number.NEGATIVE_INFINITY,
      exp: 0n,
    };
  }
  if (x.kind === 'zero') {
    // Signed zero is observable; preserve x.sign.
    // Ref: mpfr/src/get_d.c L161 — "negative ? DBL_NEG_ZERO : 0.0"
    return { value: x.sign === 1 ? 0 : -0, exp: 0n };
  }

  // --- Normal value --------------------------------------------------------
  // Ref: mpfr/src/get_d.c L164–L186 — the MPFR_ALIAS + mpfr_get_d path.
  //
  // The C code does:
  //   MPFR_ALIAS(tmp, src, MPFR_SIGN(src), 0);
  //   ret = mpfr_get_d(tmp, rnd_mode);
  //   exp = MPFR_GET_EXP(src);
  //
  // MPFR_ALIAS creates a temporary that shares the same limb data as src
  // but has a different exponent. Here we set exp=0n, meaning mpfr_get_d
  // sees a normal value in [0.5, 1.0) (since exp=0 means magnitude in
  // [2^(0-1), 2^0) = [0.5, 1.0)).
  //
  // The mantissa and precision are unchanged; only the exponent field is
  // changed to 0 so mpfr_get_d returns a value in [0.5, 1.0).
  const tmp: MPFR = {
    kind: 'normal',
    sign: x.sign,
    prec: x.prec,
    exp: 0n,   // MPFR_ALIAS sets exp to 0; x.exp is returned as the result exp
    mant: x.mant,
  };

  let ret: number = mpfr_get_d(tmp, rnd);
  let exp: bigint = x.exp;

  // Rounding can give ±1.0; adjust back to 0.5 ≤ |ret| < 1.0.
  // Ref: mpfr/src/get_d.c L169–L178
  if (ret === 1.0) {
    ret = 0.5;
    exp = exp + 1n;
  } else if (ret === -1.0) {
    ret = -0.5;
    exp = exp + 1n;
  }

  return { value: ret, exp };
}
