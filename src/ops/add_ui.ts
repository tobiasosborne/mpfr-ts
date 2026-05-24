/**
 * ops/add_ui.ts — pure-TS port of MPFR's `mpfr_add_ui`.
 *
 * Add an {@link MPFR} value `b` and an unsigned long integer `c`,
 * returning the rounded result at the target precision.
 *
 * C signature
 * -----------
 *
 *   int mpfr_add_ui(mpfr_ptr y, mpfr_srcptr x, unsigned long int u,
 *                   mpfr_rnd_t rnd_mode);
 *
 *   Ref: mpfr/src/add_ui.c L25-L94 — the full C reference body.
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_add_ui(b, c, prec, rnd) -> Result
 *
 *   - `b` is an MPFR value (any kind: normal, zero, inf, nan).
 *   - `c` is a non-negative bigint in `[0, ULONG_MAX]` (`[0, 2^64 - 1]`).
 *   - `prec` is the output precision in bits.
 *   - `rnd` is one of the five rounding modes.
 *
 * Algorithm
 * ---------
 *
 *   C structure (mpfr/src/add_ui.c L25-L94):
 *
 *   1. If `u == 0`: delegate to `mpfr_set(y, x, rnd_mode)`.
 *      Ref: mpfr/src/add_ui.c L42-L43.
 *
 *   2. If `x` is singular (NaN/Inf/±0):
 *      - NaN → set NaN, return ternary 0.
 *      - ±Inf → set same-sign Inf, return ternary 0.
 *      - ±0 (u != 0) → return mpfr_set_ui(u, prec, rnd).
 *        Ref: mpfr/src/add_ui.c L45-L60.
 *        Note: the ±0 case discards the zero sign because u > 0 is a
 *        non-negative integer; x + u == u for any signed zero x.
 *
 *   3. Normal `x` and non-zero `u`:
 *      - Build a temporary mpfr_t `uu` at `GMP_NUMB_BITS` precision with
 *        the integer value `u` (shifted so the MSB is aligned).
 *      - Call `mpfr_add(y, x, uu, rnd_mode)`.
 *        Ref: mpfr/src/add_ui.c L63-L74 (MPFR_LONG_WITHIN_LIMB branch).
 *
 *   TS divergence: we build `uu` via `mpfr_set_ui(c, bitLength(c), 'RNDN')`
 *   rather than `GMP_NUMB_BITS` precision. Since `bitLength(c)` is the
 *   exact number of significant bits in `c`, the conversion is exact
 *   (ternary 0). Using this exact precision avoids the need for exponent-
 *   range save/restore (MPFR_SAVE_EXPO_MARK / MPFR_SAVE_EXPO_FREE at
 *   mpfr/src/add_ui.c L67-L69 and L75) and produces the same final result
 *   as the C path because mpfr_add respects the source's precision.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add_ui.c L25-L94 — full C reference.
 *   - src/ops/set_ui.ts — used to materialise `c` as an exact MPFR.
 *   - src/ops/set.ts — used for the c == 0 fast path.
 *   - src/ops/add.ts — load-bearing delegate for the normal case.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md §"Hallucination-risk callouts": signed zero, ternary,
 *     rounding mode count.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  posInf,
} from "../core.ts";
import { mpfr_set } from "./set.ts";
import { mpfr_set_ui } from "./set_ui.ts";
import { mpfr_add } from "./add.ts";

/** Largest unsigned 64-bit integer value. */
const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

/**
 * Number of significant bits in a non-negative bigint. Returns 0n for 0n.
 *
 * Used to compute the exact precision for building the integer MPFR from `c`,
 * so the conversion is lossless (ternary 0 from set_ui) regardless of the
 * magnitude.
 *
 * Ref: mpfr/src/add_ui.c L70-L71 — `count_leading_zeros(cnt, u); up[0] = u << cnt;
 *   MPFR_SET_EXP(uu, GMP_NUMB_BITS - cnt);` achieves the same normalisation.
 */
function bitLength(n: bigint): bigint {
  if (n <= 0n) return 0n;
  let bits = 0n;
  let x = n;
  while (x > 0n) {
    bits++;
    x >>= 1n;
  }
  return bits;
}

/**
 * Validate the public-boundary arguments. Throws `MPFRError` on bad input.
 */
function validateArgs(c: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof c !== 'bigint') {
    throw new MPFRError('EPREC', `c must be bigint, got ${typeof c}`);
  }
  if (c < 0n || c > ULONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `c out of uint64 range [0, ${ULONG_MAX_VAL}], got ${c}`,
    );
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
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
 * Add an MPFR value and an unsigned long integer, returning the rounded
 * result at target precision.
 *
 * @mpfrName mpfr_add_ui
 *
 * @param b     the MPFR addend (any kind: normal, zero, inf, nan).
 * @param c     the unsigned integer addend, as bigint in `[0, 2^64 - 1]`.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}` per the {@link Result} shape.
 *
 * @throws {MPFRError} `EPREC` on bad precision or c outside uint64 range;
 *                    `EROUND` on unknown rounding mode.
 *
 * @example
 *   mpfr_add_ui(setD(1.5, 53n, 'RNDN').value, 2n, 53n, 'RNDN');
 *     // → {value: 3.5 at prec 53, ternary: 0}
 *   mpfr_add_ui(posZero(53n), 5n, 53n, 'RNDN');
 *     // → {value: 5.0 at prec 53, ternary: 0}
 *   mpfr_add_ui(NAN_VALUE, 5n, 53n, 'RNDN');
 *     // → {value: NaN, ternary: 0}
 */
export function mpfr_add_ui(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(c, prec, rnd);

  // --- Fast path: c == 0 ---------------------------------------------------
  // mpfr/src/add_ui.c L42-L43: `if (u == 0) return mpfr_set(y, x, rnd_mode);`
  // The result is just a rounded copy of b at the target precision.
  if (c === 0n) {
    return mpfr_set(b, prec, rnd);
  }

  // --- Singular b: NaN / ±Inf / ±0 -----------------------------------------
  // mpfr/src/add_ui.c L45-L60: the "minor optimization" singular dispatch.

  // NaN propagation: NaN + anything → NaN. Ternary 0.
  // mpfr/src/add_ui.c L47-L51: `if (MPFR_IS_NAN(x)) { MPFR_SET_NAN(y); MPFR_RET_NAN; }`
  if (b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // ±Inf + finite integer → ±Inf (sign preserved). Ternary 0 (infinity is exact).
  // mpfr/src/add_ui.c L52-L57: `if (MPFR_IS_INF(x)) { ... MPFR_RET(0); }`
  if (b.kind === 'inf') {
    return {
      value: b.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // ±0 + c (c != 0) → mpfr_set_ui(c, prec, rnd).
  // mpfr/src/add_ui.c L58-L60: `MPFR_ASSERTD(MPFR_IS_ZERO(x) && u != 0);
  //   return mpfr_set_ui(y, u, rnd_mode);`
  // The signed zero's sign is discarded since c > 0 is unsigned — the result
  // is just the integer c at the target precision. Signed zero sign is
  // irrelevant here: b + c == c for any ±0 b and non-zero c.
  if (b.kind === 'zero') {
    return mpfr_set_ui(c, prec, rnd);
  }

  // --- Normal b + non-zero c -----------------------------------------------
  // mpfr/src/add_ui.c L63-L74 (MPFR_LONG_WITHIN_LIMB branch):
  //   Build a temporary mpfr_t `uu` at GMP_NUMB_BITS precision with the
  //   integer value `u` (MSB-aligned), then delegate to mpfr_add.
  //
  // TS equivalent: build `uMPFR = mpfr_set_ui(c, bitLength(c), 'RNDN').value`.
  // Since bitLength(c) is the exact significant-bit count of c, the
  // conversion is lossless (ternary 0), and we avoid the exponent-range
  // save/restore the C path needs around mpfr_add.
  //
  // Ref: mpfr/src/add_ui.c L70-L73 — `count_leading_zeros` normalises u
  //   to GMP_NUMB_BITS; we achieve the same net result with bitLength.
  const cPrec = bitLength(c); // exact number of bits in c; c > 0 so cPrec >= 1
  const uMPFR = mpfr_set_ui(c, cPrec, 'RNDN').value;

  return mpfr_add(b, uMPFR, prec, rnd);
}
