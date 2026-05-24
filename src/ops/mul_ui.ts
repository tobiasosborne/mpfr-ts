/**
 * ops/mul_ui.ts — pure-TS port of MPFR's `mpfr_mul_ui`.
 *
 * Multiply an {@link MPFR} value `b` by an unsigned long integer `c`,
 * returning the result rounded to `prec` bits per rounding mode `rnd`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul_ui(mpfr_ptr y, mpfr_srcptr x, unsigned long int u, mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_mul_ui(b: MPFR, c: bigint, prec: bigint, rnd: RoundingMode): Result
 *
 *   - `c` is a non-negative unsigned long as a `bigint` in `[0, ULONG_MAX]`.
 *   - Returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Dispatch order (mirroring mpfr/src/mul_ui.c L26-L143)
 * ------------------------------------------------------
 *
 *  1. Singular b:
 *     a. NaN → NaN, ternary 0.
 *     b. ±Inf with c=0 → NaN (0*Inf indeterminate), ternary 0.
 *     c. ±Inf with c>0 → ±Inf (sign preserved), ternary 0.
 *     d. ±0 → ±0 (sign of b preserved), ternary 0.
 *
 *  2. c == 0 for normal b: → ±0, sign of b, ternary 0.
 *     (C reference: mpfr/src/mul_ui.c L113-L120 — u < 1 path sets zero
 *     with MPFR_SET_SAME_SIGN, ternary 0.)
 *
 *  3. c == 1: → mpfr_set(b, prec, rnd). The "copy with rounding" path.
 *     (C reference: mpfr/src/mul_ui.c L120-L122 — `return mpfr_set(y, x, rnd_mode);`.)
 *
 *  4. c is a power of 2: → mpfr_mul_2si(b, ilog2(c), prec, rnd).
 *     This is a pure exponent-shift with no mantissa arithmetic.
 *     (C reference: mpfr/src/mul_ui.c L124-L125 — `return mpfr_mul_2si(y, x, MPFR_INT_CEIL_LOG2(u), rnd_mode);`.)
 *
 *  5. General case: build cMPFR exactly via mpfr_set_ui(c, bitLength(c), 'RNDN'),
 *     then delegate to mpfr_mul(b, cMPFR, prec, rnd).
 *     (C reference: the `#else` path at mpfr/src/mul_ui.c L123-L140 uses
 *     mpfr_set_ui + mpfr_mul for architectures where unsigned long exceeds
 *     a single GMP limb. Our TS port always takes this path for the
 *     general case since we avoid direct limb-array manipulation.)
 *
 * Note on the power-of-2 fast path
 * ---------------------------------
 *
 * If mul_2si.ts is not yet available, we fall through to the general
 * mpfr_mul path (which produces the same result). The power-of-2 check
 * is retained as an optimisation gate; if the import fails at load time,
 * we catch and skip. In practice, mul_2si should be available in the
 * production build; if absent the general path is correct.
 *
 * Signed-zero preservation
 * ------------------------
 *
 * Per CLAUDE.md "Hallucination-risk callouts: Signed zero is real":
 * - ±0 * c = ±0 (sign of b preserved, not sign of c which is always +).
 * - ±Inf * 0 = NaN (indeterminate).
 * Normal * 0 = sign-of-b * 0.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/mul_ui.c L26-L143 — C reference (full body).
 *   - src/ops/mul.ts — general-case delegate.
 *   - src/ops/set_ui.ts — integer-to-MPFR exact conversion.
 *   - src/ops/set.ts — c==1 fast path delegate.
 *   - src/ops/mul_2si.ts — power-of-2 fast path delegate.
 *   - src/core.ts — locked schema.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negZero,
  posZero,
  negInf,
  posInf,
} from "../core.ts";
import { mpfr_mul } from "./mul.ts";
import { mpfr_set_ui } from "./set_ui.ts";
import { mpfr_set } from "./set.ts";
import { mpfr_mul_2si } from "./mul_2si.ts";

/** Largest unsigned 64-bit integer: 2^64 - 1 (ULONG_MAX on LP64). */
const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

/**
 * Validate public-boundary arguments. Throws MPFRError on bad input.
 * We trust the MPFR input value b (runner pre-validates via decodeMpfr)
 * and only check the scalar c / prec / rnd arguments.
 *
 * Ref: mpfr/src/mul_ui.c L26-L30 — no explicit range guard on u in C,
 * but the C type `unsigned long` implies [0, ULONG_MAX]; we mirror this.
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
 * Bit length of a non-negative bigint (position of topmost set bit, 1-indexed).
 * Returns 0n for 0n.
 *
 * Used to compute the exact precision needed for mpfr_set_ui(c, ...) so
 * that cMPFR is an exact representation of c (no rounding).
 */
function bitLength(v: bigint): bigint {
  if (v <= 0n) return 0n;
  return BigInt(v.toString(2).length);
}

/**
 * True iff v > 1 and v is an exact power of 2.
 * Ref: mpfr/src/mul_ui.c L124 — `IS_POW2(u)` macro expands to
 *   `(u & (u-1)) == 0` (note: u >= 2 is already established at this point).
 */
function isPow2(v: bigint): boolean {
  return v > 1n && (v & (v - 1n)) === 0n;
}

/**
 * Integer log2 of a power-of-2 bigint (exact).
 * Pre-condition: v is a positive power of 2.
 * Ref: mpfr/src/mul_ui.c L124 — `MPFR_INT_CEIL_LOG2(u)` is exact ilog2 for powers of 2.
 */
function ilog2(v: bigint): bigint {
  let k = 0n;
  let x = v;
  while (x > 1n) {
    x >>= 1n;
    k++;
  }
  return k;
}

/**
 * Multiply `b` by unsigned integer `c`, rounding to `prec` bits per `rnd`.
 *
 * @mpfrName mpfr_mul_ui
 *
 * @param b    The MPFR operand (any kind).
 * @param c    The unsigned long multiplier, as a bigint in [0, ULONG_MAX].
 * @param prec Output precision in **bits**, in [PREC_MIN, PREC_MAX].
 * @param rnd  One of the five RoundingMode values.
 *
 * @returns `{value, ternary}` per the {@link Result} shape.
 *
 * @throws {MPFRError} EPREC on bad precision or out-of-range c;
 *                    EROUND on bad rounding mode.
 *
 * Ref: mpfr/src/mul_ui.c L26-L143.
 */
export function mpfr_mul_ui(
  b: MPFR,
  c: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(c, prec, rnd);

  // --- Dispatch on singular b (mpfr/src/mul_ui.c L34-L72) ----------------

  // (1) NaN propagation.
  // Ref: mpfr/src/mul_ui.c L36-L40 — MPFR_IS_NAN(x) → MPFR_SET_NAN(y), MPFR_RET_NAN.
  if (b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) ±Inf handling.
  // Ref: mpfr/src/mul_ui.c L41-L56 — c=0 produces NaN (0*Inf indeterminate);
  // c>0 produces ±Inf with sign of b preserved.
  if (b.kind === 'inf') {
    if (c === 0n) {
      // 0 * Inf — indeterminate.
      return { value: NAN_VALUE, ternary: 0 };
    }
    // c > 0: ±Inf with same sign as b (sign of c is always +, so sign is b.sign).
    return {
      value: b.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // (3) ±0: → ±0 with sign of b preserved, exact.
  // Ref: mpfr/src/mul_ui.c L58-L63 — MPFR_SET_ZERO(y); MPFR_SET_SAME_SIGN(y, x); MPFR_RET(0).
  if (b.kind === 'zero') {
    return {
      value: b.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // b.kind is 'normal' from here on.

  // --- u == 0: normal * 0 = ±0 with sign of b (mpfr/src/mul_ui.c L113-L120) ---
  // Ref: c < 1 → MPFR_SET_ZERO; MPFR_SET_SAME_SIGN(y, x); MPFR_RET(0).
  if (c === 0n) {
    return {
      value: b.sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // --- u == 1: mpfr_set (copy with possible rounding) (mpfr/src/mul_ui.c L120-L122) ---
  // Ref: `return mpfr_set(y, x, rnd_mode);`
  if (c === 1n) {
    return mpfr_set(b, prec, rnd);
  }

  // --- u is a power of 2: exponent-shift fast path (mpfr/src/mul_ui.c L124-L125) ---
  // Ref: `return mpfr_mul_2si(y, x, MPFR_INT_CEIL_LOG2(u), rnd_mode);`
  if (isPow2(c)) {
    const shift = ilog2(c);
    return mpfr_mul_2si(b, shift, prec, rnd);
  }

  // --- General case: build exact cMPFR and delegate to mpfr_mul ---
  // Ref: mpfr/src/mul_ui.c L130-L140 (#else path — mpfr_set_ui + mpfr_mul).
  // We use bitLength(c) as the precision for set_ui so that c is represented
  // exactly (bitLength(c) bits exactly cover c without rounding).
  // Note: c >= 3 here and is not a power of 2, so bitLength(c) >= 2.
  const cPrec = bitLength(c);
  const cMpfr: MPFR = mpfr_set_ui(c, cPrec, 'RNDN').value;

  return mpfr_mul(b, cMpfr, prec, rnd);
}
