/**
 * ops/set_d.ts — pure-TS port of MPFR's `mpfr_set_d`.
 *
 * Convert an IEEE 754 binary64 (`number`) to an {@link MPFR} value at the
 * caller-supplied precision, with correct rounding per the rounding mode,
 * returning the canonical `{value, ternary}` shape.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_d(mpfr_t rop, double op, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - returns the ternary as the function result.
 *
 *   Ref: mpfr/src/set_d.c L241–L324.
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_d(d: number, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `prec` as an explicit positional argument (no `rop`);
 *   - returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Algorithm
 * ---------
 *
 * Three phases — special-value handling, IEEE 754 extraction, rounding.
 *
 *   1. Specials. NaN / ±Inf / ±0 short-circuit to the corresponding
 *      singular MPFR with `ternary === 0` (the conversion is exact for
 *      every special — there's no real-number to round). Signed zero is
 *      observable in MPFR (CLAUDE.md "Signed zero is real"), so we
 *      distinguish `+0` and `-0` via `Object.is(d, -0)` rather than the
 *      direction-blind `d === 0` test.
 *
 *   2. IEEE 754 extraction. Use a shared 8-byte buffer wrapped by both
 *      `Float64Array` and `DataView`: writing the double into the
 *      `Float64Array` slot and reading the resulting bits out as two
 *      `Uint32` words is the standard portable way to inspect a `number`'s
 *      bit pattern in JS. The format (binary64 from IEEE 754-2019 §3.4):
 *
 *         sign      : 1 bit at position 63
 *         exponent  : 11 bits at positions 52–62, biased by 1023
 *         mantissa  : 52 bits at positions 0–51 (with implicit MSB 1 for
 *                     normal values, no implicit MSB for subnormals)
 *
 *      A normal double's value is
 *
 *         (-1)^sign * (1 + mant52 * 2^-52) * 2^(exp_biased - 1023)
 *
 *      We promote it to the MPFR convention `sign * mant53 * 2^(exp-53)`
 *      with a 53-bit MSB-aligned mantissa by prepending the implicit 1
 *      bit. The MPFR unbiased exponent satisfies `|value| ∈ [2^(exp-1),
 *      2^exp)`, so for a normal double `exp_mpfr = exp_biased - 1023 + 1
 *      = exp_biased - 1022` (matching `extract_double`'s `exp -= 1022` at
 *      mpfr/src/set_d.c L79 — note MPFR's `extract_double` works in
 *      `[1/2, 1)` rather than `[1, 2)`, hence the off-by-one in the
 *      exponent base; the schema in src/core.ts uses the `[2^(exp-1),
 *      2^exp)` convention).
 *
 *      Subnormals (exponent field == 0, non-zero mantissa) have no implicit
 *      leading 1. We renormalise: find the position of the topmost set bit,
 *      shift the mantissa left until that bit is at position 52 (i.e. the
 *      implicit-1 slot), and adjust the exponent accordingly. The bias
 *      offset for subnormals is fixed at -1021 (the C side: `exp = -1021`
 *      at mpfr/src/set_d.c L84) — the smallest normal binary64 has
 *      biased exponent 1 → unbiased -1022, and a subnormal of value
 *      `2^-1022 * (m * 2^-52)` with leading bit at position k satisfies
 *      |value| ∈ [2^(-1022-52+k), 2^(-1022-52+k+1)), giving MPFR exp =
 *      -1022-52+k+1 = -1073+k. The renormalisation loop computes the
 *      equivalent.
 *
 *   3. Rounding. The extracted (sign, exp, mant53) is exact: the
 *      53-bit mantissa is the full information in a binary64. If
 *      `prec >= 53n`, padding with zeros to the requested precision is
 *      lossless (ternary === 0). If `prec < 53n`, we round 53→prec bits
 *      per `rnd`, computing the ternary as `sign of (rounded - exact)`
 *      — emphatically NOT `sign of (exact - rounded)`, the most common
 *      subtle bug (CLAUDE.md hallucination-risk callout).
 *
 *      The round step may cause a carry-out (e.g. 1.999...→2 in some
 *      rounding modes), which would push the mantissa MSB one position
 *      higher and increment the exponent by 1. We handle that explicitly
 *      after the round.
 *
 * Why inline the rounding here (not a substrate helper)
 * -----------------------------------------------------
 *
 * MPFR has a general `mpfr_round_raw` substrate primitive. We don't have
 * a TS analog yet — this port is the second public-surface op and the
 * first one to need rounding. Extracting `round_raw` from this port the
 * moment a third caller needs the same logic is the right time; doing
 * it pre-emptively before the contract is exercised would be
 * speculation. For now the rounding is local; a TODO marker (mpfr-ts
 * Production exit criterion) flags the extraction.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_d.c — C reference (extract_double + dispatch).
 *   - mpfr/src/round_raw_generic.c — the canonical rounding primitive
 *     in MPFR; the in-line rounding here mirrors its bit-test logic for
 *     RNDN / RNDZ / RNDU / RNDD / RNDA.
 *   - src/core.ts §"validate" — output invariants.
 *   - eval/functions/mpfr_set_d/spec.json — class, signature, divergence.
 *   - CLAUDE.md Law 4 — schema imports; "Hallucination-risk callouts" —
 *     signed zero, ternary direction, rounding-mode count.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
} from '../core.ts';

// ---------------------------------------------------------------------------
// IEEE 754 bit-pattern extraction
// ---------------------------------------------------------------------------

/**
 * Shared 8-byte buffer for double <-> bits conversion. The two views
 * alias the SAME backing memory, so a write through one is observable
 * through the other. Module-level (one allocation per process) — the
 * port is sync and single-threaded within a worker, so there's no
 * re-entrancy concern.
 *
 * Endianness: `DataView` defaults to big-endian on get/set methods, but
 * we pass `littleEndian = true` explicitly — every IEEE 754 host this
 * code targets (x86_64, aarch64) is little-endian, and being explicit
 * removes the host-endianness dependency from the algorithm.
 */
const F64_BUF = new ArrayBuffer(8);
const F64_VIEW = new Float64Array(F64_BUF);
const U32_VIEW = new DataView(F64_BUF);

/**
 * Extracted IEEE 754 components for a finite, nonzero double.
 *
 * - `sign`: 1 or -1, matching {@link Sign}.
 * - `expMpfr`: unbiased MPFR-convention exponent. The value's magnitude
 *   lies in `[2^(expMpfr - 1), 2^expMpfr)`.
 * - `mant53`: the 53-bit MSB-aligned mantissa as a bigint, with
 *   `2^52 <= mant53 < 2^53`. The implicit-1 has been promoted to an
 *   explicit MSB for both normals and renormalised subnormals.
 */
interface DoubleParts {
  readonly sign: Sign;
  readonly expMpfr: bigint;
  readonly mant53: bigint;
}

/**
 * Decompose a finite, nonzero double into MPFR-convention parts.
 *
 * Pre-condition: `Number.isFinite(d) && d !== 0`. The caller has already
 * dispatched NaN / ±Inf / ±0 to the special-value paths; this helper does
 * not re-validate (would be wasted work — the dispatch is one if-chain
 * away in {@link mpfr_set_d}).
 */
function extractDouble(d: number): DoubleParts {
  F64_VIEW[0] = d;
  // Read the two 32-bit halves as little-endian unsigned integers. `lo`
  // holds bits 0..31 of the IEEE 754 binary64 (the low 32 mantissa bits);
  // `hi` holds bits 32..63 (the sign, exponent, and high 20 mantissa
  // bits). We then assemble a 64-bit bigint from those halves.
  const lo = U32_VIEW.getUint32(0, /* littleEndian */ true);
  const hi = U32_VIEW.getUint32(4, /* littleEndian */ true);

  // bits = (hi << 32) | lo, all as bigint.
  const bits = (BigInt(hi) << 32n) | BigInt(lo);

  // Extract sign (top bit), biased exponent (next 11 bits), 52-bit
  // mantissa fraction (low 52 bits).
  const signBit = bits >> 63n;
  const expBiased = Number((bits >> 52n) & 0x7ffn);
  const mantFrac = bits & ((1n << 52n) - 1n);
  const sign: Sign = signBit === 0n ? 1 : -1;

  if (expBiased === 0) {
    // Subnormal: no implicit leading 1, value = sign * mantFrac *
    // 2^(-1022 - 52). To MSB-align to 53 bits we count the leading
    // zeros in the 52-bit mantFrac and shift left. mantFrac is
    // guaranteed nonzero here (else d would be ±0 and the caller would
    // have short-circuited).
    //
    // bigint has no built-in count-leading-zeros; the manual loop here
    // is bounded by 52 iterations — cheap. We compute `bitLen` (the
    // position of the topmost set bit, 1-indexed) and then shift left
    // by `(53 - bitLen)` so the topmost bit lands at position 52 (i.e.
    // the new mantissa is 2^52 + lower_bits, exactly what a normal
    // double's mantissa looks like).
    let bitLen = 0;
    let probe = mantFrac;
    while (probe > 0n) {
      bitLen++;
      probe >>= 1n;
    }
    // `bitLen` is in [1, 52]. Shift left by (53 - bitLen) so the MSB
    // sits at position 52.
    const shift = BigInt(53 - bitLen);
    const mant53 = mantFrac << shift;
    // Exponent: the subnormal's IEEE 754 value (when expBiased == 0) is
    // sign * 0.mantFrac_b * 2^-1022 = sign * mantFrac * 2^-1074
    // (since mantFrac is the integer reading of the 52 fraction bits;
    // `0.mantFrac_b` = mantFrac / 2^52 = mantFrac * 2^-52, and times
    // 2^-1022 gives 2^-1074). We renormalised by shifting left by
    // `shift = 53 - bitLen`, so mant53 = mantFrac << shift, meaning the
    // value is mant53 * 2^(-1074 - shift).
    //
    // The schema convention (src/core.ts) writes a normal value as
    //
    //   sign * mant * 2^(exp - prec)        with prec = 53, mant in [2^52, 2^53)
    //
    // so matching the two forms: mant53 * 2^(-1074 - shift)
    //                          = mant53 * 2^(exp_mpfr - 53)
    // gives exp_mpfr = -1074 - shift + 53 = -1021 - shift
    //                                    = -1021 - (53 - bitLen)
    //                                    = -1074 + bitLen.
    //
    // Sanity: for the smallest subnormal `Number.MIN_VALUE = 2^-1074`,
    // mantFrac = 1n, bitLen = 1, shift = 52, mant53 = 2^52, exp_mpfr =
    // -1073. Check: mant53 * 2^(exp_mpfr - prec) = 2^52 * 2^(-1073-53)
    // = 2^52 * 2^-1126 = 2^-1074. ✓
    //
    // For DBL_MIN/2 = 2^-1023: mantFrac = 2^51, bitLen = 52, shift = 1,
    // mant53 = 2^52, exp_mpfr = -1022. Check: 2^52 * 2^(-1022-53) =
    // 2^52 * 2^-1075 = 2^-1023. ✓
    const expMpfr = BigInt(-1074 + bitLen);
    return { sign, expMpfr, mant53 };
  }

  // Normal: prepend the implicit-1 bit at position 52 to get a 53-bit
  // mantissa. The value is sign * mant53 * 2^(expBiased - 1023 - 52);
  // converting to the schema's `[2^(exp-1), 2^exp)` form (mant in
  // [2^52, 2^53) → magnitude in [2^(expBiased-1023), 2^(expBiased-1022)))
  // gives MPFR exp = expBiased - 1022.
  //
  // The `expBiased === 2047` case (Inf/NaN) is unreachable: the caller
  // has already dispatched. We do NOT re-check; a defensive assertion
  // would be load-bearing for security-critical code but is noise here.
  const mant53 = (1n << 52n) | mantFrac;
  const expMpfr = BigInt(expBiased - 1022);
  return { sign, expMpfr, mant53 };
}

// ---------------------------------------------------------------------------
// Rounding
// ---------------------------------------------------------------------------

/**
 * Round-step result. The result is the same shape used at the call site
 * to build the final `MPFR` value, plus a ternary flag computed against
 * the unrounded source.
 *
 * - `mant`: the rounded mantissa, MSB-aligned to `outPrec` bits (i.e.
 *   in `[2^(outPrec-1), 2^outPrec)`).
 * - `exp`: the post-round MPFR exponent. May be the source exponent or
 *   source exponent + 1 (the carry-from-MSB case).
 * - `ternary`: -1 if rounded < exact, 0 if equal, +1 if rounded > exact,
 *   sign-of-(rounded-exact) per CLAUDE.md.
 */
interface RoundedMantissa {
  readonly mant: bigint;
  readonly exp: bigint;
  readonly ternary: Ternary;
}

/**
 * Round a source-precision mantissa down to `outPrec` bits per `rnd`.
 *
 * Pre-conditions:
 *   - `srcMant >= 2^(srcPrec - 1)` and `srcMant < 2^srcPrec` — MSB-aligned.
 *   - `srcPrec > outPrec` — the lossless `srcPrec <= outPrec` case is
 *     handled by the caller without entering this function.
 *   - `outPrec >= 1`.
 *   - `sign` is the sign of the unrounded value; it determines the
 *     RNDU/RNDD branch.
 *   - `srcExp` is the MPFR exponent of the unrounded value (mantissa
 *     MSB at position `srcPrec-1` represents magnitude `2^(srcExp-1)`).
 *
 * Algorithm (mirrors mpfr/src/round_raw_generic.c logic in compact form):
 *
 *   Let k = srcPrec - outPrec (the number of low bits to drop).
 *   Let trunc = srcMant >> k (the truncated mantissa, in [2^(outPrec-1),
 *     2^outPrec)).
 *   Let dropped = srcMant & (2^k - 1) (the bits we're dropping).
 *
 *   `exact` ↔ `dropped == 0`. Otherwise:
 *
 *     - RNDZ: keep trunc; ternary = sign-of(-1*sign) (toward zero
 *       always undershoots magnitude, so for sign=+1 trunc < exact, ternary=-1;
 *       for sign=-1 trunc > exact, ternary=+1). This is "truncation".
 *
 *     - RNDA: round away from zero — add 1 to trunc (LSB increment).
 *       For sign=+1, ternary=+1; for sign=-1, ternary=-1.
 *
 *     - RNDD: round toward -∞. If sign=+1, truncate (same as RNDZ); if
 *       sign=-1, round away from zero (same as RNDA).
 *
 *     - RNDU: round toward +∞. If sign=+1, away; if sign=-1, truncate.
 *
 *     - RNDN: round to nearest, ties to even. The "tie" is dropped ==
 *       2^(k-1) exactly (the half-ulp boundary). Strictly above the
 *       half: increment. Strictly below: truncate. Tie: increment iff
 *       trunc's LSB is 1 (i.e. break to even, which is trunc with LSB 0).
 *
 *   Carry-out handling: incrementing trunc may overflow past 2^outPrec.
 *   That happens iff the pre-increment trunc was 2^outPrec - 1 (all bits
 *   set). The post-increment value is 2^outPrec — we right-shift it by
 *   one (becoming 2^(outPrec-1)) and bump the exponent by 1. The new
 *   value is still in [2^(outPrec-1), 2^outPrec).
 *
 * Ref: mpfr/src/round_raw_generic.c — the canonical bit-test logic for
 *   all five modes, generalised for arbitrary-precision MPFR mantissas.
 *   Our k-bit `dropped` here is a single bigint vs the C side's limb
 *   array, but the rounding decisions are identical.
 */
function roundMantissa(
  srcMant: bigint,
  srcPrec: bigint,
  srcExp: bigint,
  outPrec: bigint,
  sign: Sign,
  rnd: RoundingMode,
): RoundedMantissa {
  const k = srcPrec - outPrec;
  const kBig = k;
  const trunc = srcMant >> kBig;
  const droppedMask = (1n << kBig) - 1n;
  const dropped = srcMant & droppedMask;

  if (dropped === 0n) {
    // Exact: the low k bits are all zero, so truncation IS the exact
    // value. Ternary is 0; no carry possible.
    return { mant: trunc, exp: srcExp, ternary: 0 };
  }

  // Decide whether to increment based on rnd, sign, dropped, and trunc's
  // LSB (for RNDN tie-breaking).
  let increment: boolean;
  switch (rnd) {
    case 'RNDZ':
      // Toward zero: never increment.
      increment = false;
      break;
    case 'RNDA':
      // Away from zero: always increment when dropped != 0.
      increment = true;
      break;
    case 'RNDD':
      // Toward -∞: increment iff negative (so the magnitude grows, and
      // the signed value drops below the exact). Positive truncates.
      increment = sign === -1;
      break;
    case 'RNDU':
      // Toward +∞: increment iff positive. Negative truncates (the
      // magnitude shrinks, so the signed value rises above the exact).
      increment = sign === 1;
      break;
    case 'RNDN': {
      // Half-ulp boundary is 2^(k-1). dropped > half → round up;
      // dropped < half → round down; dropped == half (the "tie") →
      // round to even (round up iff trunc's LSB is 1).
      const half = 1n << (kBig - 1n);
      if (dropped > half) {
        increment = true;
      } else if (dropped < half) {
        increment = false;
      } else {
        // Tie: ties-to-even.
        increment = (trunc & 1n) === 1n;
      }
      break;
    }
    default: {
      // Unreachable under TS narrowing — but the caller takes `rnd` as a
      // potentially-untrusted `RoundingMode` (the wire format is the
      // boundary), so we surface a precise error rather than fall through.
      const _exhaustive: never = rnd;
      void _exhaustive;
      throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
    }
  }

  if (!increment) {
    // Truncating: rounded magnitude < exact magnitude. Ternary is the
    // sign of (rounded - exact). For sign=+1: rounded < exact → ternary=-1.
    // For sign=-1: rounded > exact (less negative) → ternary=+1.
    return {
      mant: trunc,
      exp: srcExp,
      ternary: sign === 1 ? -1 : 1,
    };
  }

  // Incrementing: rounded magnitude > exact magnitude.
  const incremented = trunc + 1n;
  const upperBound = 1n << outPrec;
  if (incremented === upperBound) {
    // Carry-out: the rounded value is exactly 2^outPrec, but the
    // MSB-alignment invariant requires `< 2^outPrec`. Renormalise by
    // shifting right one position (becoming 2^(outPrec-1)) and bumping
    // the exponent: same numeric value, valid storage form.
    return {
      mant: upperBound >> 1n,
      exp: srcExp + 1n,
      ternary: sign === 1 ? 1 : -1,
    };
  }
  return {
    mant: incremented,
    exp: srcExp,
    ternary: sign === 1 ? 1 : -1,
  };
}

// ---------------------------------------------------------------------------
// Validation helpers
// ---------------------------------------------------------------------------

/**
 * Validate `prec` and `rnd` at the public boundary. Throws `MPFRError`
 * on bad input (`EPREC` for precision, `EROUND` for rounding mode). The
 * special-value paths and `extractDouble` both rely on this having
 * already run.
 *
 * We do NOT defer the precision check to {@link posZero}/{@link posInf}
 * — those functions also throw `EPREC`, but only after we've already
 * paid the special-value dispatch cost. Front-loading the check makes
 * the error point at `mpfr_set_d` rather than at a constructor it
 * happens to call internally.
 */
function validateArgs(prec: bigint, rnd: RoundingMode): void {
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

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Convert a JavaScript `number` (IEEE 754 binary64) to an {@link MPFR}
 * value at `prec` bits, rounded per `rnd`.
 *
 * @mpfrName mpfr_set_d
 *
 * @param d     the IEEE 754 binary64 value. NaN, ±Infinity, and ±0 are
 *              all handled; signed zero is preserved (CLAUDE.md
 *              "Signed zero is real").
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`. Per the
 *              hallucination-risk callout, `53n` is 53 mantissa bits
 *              (= IEEE float64 width), not 53 decimal digits.
 * @param rnd   one of the five rounding modes in {@link RoundingMode}.
 *              `RNDF` and the retired `RNDNA` are not supported.
 *
 * @returns     a {@link Result} pair `{value, ternary}` where:
 *              - `value` is a well-formed {@link MPFR} at the requested
 *                precision (passes `validate()` without post-processing);
 *              - `ternary` is the sign of `(value - exact)` — `0` for
 *                lossless conversions (including all specials), `±1`
 *                otherwise. Direction matters: do NOT abs() this value.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf input is NOT an error — they convert
 *                    to the corresponding MPFR singular value.
 *
 * @example
 *   mpfr_set_d(0, 53n, 'RNDN').value;      // posZero(53n)
 *   mpfr_set_d(-0, 53n, 'RNDN').value;     // negZero(53n) — Object.is on -0
 *   mpfr_set_d(NaN, 53n, 'RNDN').value;    // NAN_VALUE (prec discarded)
 *   mpfr_set_d(Infinity, 53n, 'RNDN');     // {value: posInf(53n), ternary: 0}
 *   mpfr_set_d(1.0, 53n, 'RNDN');          // exact: ternary = 0
 *   mpfr_set_d(0.1, 53n, 'RNDN');          // exact at 53 bits (the double IS the rounded value)
 *   mpfr_set_d(5.0, 2n, 'RNDN');           // 5 = 0b101 → round to 2 bits → 4 (ties-to-even)
 */
export function mpfr_set_d(d: number, prec: bigint, rnd: RoundingMode): Result {
  // Boundary validation first. Throws on bad prec / rnd; never on a
  // bad `d` (every double is a valid input).
  validateArgs(prec, rnd);

  // --- Specials ------------------------------------------------------------
  // NaN: precision is irrelevant in the TS NaN convention (prec=0n,
  // sign=1n). Ternary 0 — there's no exact real to compare against.
  if (Number.isNaN(d)) {
    return { value: NAN_VALUE, ternary: 0 };
  }
  if (d === Number.POSITIVE_INFINITY) {
    return { value: posInf(prec), ternary: 0 };
  }
  if (d === Number.NEGATIVE_INFINITY) {
    return { value: negInf(prec), ternary: 0 };
  }
  if (d === 0) {
    // Signed-zero discrimination. `d === 0` matches both +0 and -0
    // (IEEE 754 §5.11.1: zeros are equal under `=`), so we use
    // `Object.is` to distinguish them. `1/d` also works (`1/-0 ===
    // -Infinity`) but Object.is is the explicit, intent-named form.
    if (Object.is(d, -0)) {
      return { value: negZero(prec), ternary: 0 };
    }
    return { value: posZero(prec), ternary: 0 };
  }

  // --- Finite, nonzero double ----------------------------------------------
  const { sign, expMpfr, mant53 } = extractDouble(d);

  // Lossless path: prec >= 53 means we can keep the 53-bit mantissa
  // exactly and pad with zeros to widen it to `prec` bits MSB-aligned.
  // The padded mantissa is mant53 << (prec - 53), and the exponent is
  // unchanged (the value's magnitude doesn't move when we left-shift
  // the mantissa and account for the prec growth: see src/core.ts
  // value formula `sign * mant * 2^(exp - prec)`).
  if (prec >= 53n) {
    const padShift = prec - 53n;
    const paddedMant = mant53 << padShift;
    const value: MPFR = {
      kind: 'normal',
      sign,
      prec,
      exp: expMpfr,
      mant: paddedMant,
    };
    return { value, ternary: 0 };
  }

  // Lossy path: round the 53-bit mantissa down to `prec` bits per `rnd`.
  // roundMantissa handles the carry-out case where rounding pushes the
  // mantissa past 2^prec (incrementing the exponent).
  const { mant, exp, ternary } = roundMantissa(
    mant53,
    53n,
    expMpfr,
    prec,
    sign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
