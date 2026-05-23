/**
 * ops/get_d.ts — pure-TS port of MPFR's `mpfr_get_d`.
 *
 * Convert an {@link MPFR} value to the IEEE 754 binary64 closest to it
 * under the given rounding mode. The inverse of `mpfr_set_d`.
 *
 * C signature
 * -----------
 *
 *   double mpfr_get_d(mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Returns the IEEE 754 double closest to `op` rounded per `rnd`. No
 *   ternary is returned — MPFR's get-conversion family discards it. See
 *   mpfr/src/get_d.c L34–L132.
 *
 * TS signature
 * ------------
 *
 *   mpfr_get_d(x: MPFR, rnd: RoundingMode): number;
 *
 *   - takes the immutable {@link MPFR} from src/core.ts;
 *   - returns a bare JS `number` (no Result wrapper — the get-family
 *     has no ternary, per the locked schema and the C reference).
 *
 * Algorithm
 * ---------
 *
 *   1. Specials.
 *        NaN     → NaN
 *        +Inf    → +Infinity
 *        -Inf    → -Infinity
 *        ±0      → ±0 (sign preserved; signed zero is observable in MPFR)
 *
 *   2. Overflow (magnitude above DBL_MAX).
 *      The largest finite binary64 is DBL_MAX = (2^53 - 1) * 2^971 with
 *      MPFR exponent 1024 and mantissa (2^53 - 1) << (prec - 53). Anything
 *      with `exp > 1024` overflows. Rounding-mode-dependent target:
 *        positive:  RNDZ/RNDD → +DBL_MAX, others → +Infinity
 *        negative:  RNDZ/RNDU → -DBL_MAX, others → -Infinity
 *      (Mirrors mpfr/src/get_d.c L83–L90 exactly.)
 *
 *   3. Underflow (magnitude below half the smallest subnormal).
 *      The smallest representable positive binary64 is 2^-1074 (subnormal).
 *      The "no-double-survives" threshold is `exp < -1073` per the C
 *      reference (mpfr/src/get_d.c L66) — values strictly below 2^-1074
 *      in magnitude. Rounding-mode-dependent target:
 *        positive:  RNDU → 2^-1074 (or RNDN if magnitude > 2^-1075), else +0
 *        negative:  RNDD → -2^-1074 (or RNDN if magnitude > 2^-1075), else -0
 *      RNDA is folded to RNDU/RNDD by sign at the entry. (Mirrors
 *      mpfr/src/get_d.c L66–L81.)
 *
 *   4. In-range (-1073 <= exp <= 1024).
 *      We need to produce a 53-bit (or fewer, for subnormals) mantissa
 *      rounded from the source `prec`-bit MSB-aligned mantissa per `rnd`.
 *      Two sub-cases:
 *
 *        - Normal-target range (exp >= -1021): output is a normal double,
 *          53 significant bits, biased exponent `exp + 1022` in [1, 2046].
 *          We round the source mantissa to 53 bits using the same
 *          roundMantissa primitive set_d.ts uses (the call here is in the
 *          OPPOSITE direction — srcPrec=prec, outPrec=53 — but the
 *          rounding logic is symmetric in the prec arguments).
 *
 *        - Subnormal-target range (-1073 <= exp < -1021): output is a
 *          subnormal double with fewer than 53 significant bits — exactly
 *          `nbits = 53 + 1021 + exp - 1 = 1073 + exp` bits, per
 *          mpfr/src/get_d.c L101 (`nbits += 1021 + e`, where C's `e` is
 *          MPFR's `exp` minus the off-by-one between `[1/2, 1)` and
 *          `[2^(exp-1), 2^exp)`; we re-derive directly in our convention
 *          to avoid the C off-by-one). We round the source mantissa to
 *          `nbits` bits then assemble the subnormal double directly.
 *
 *      Carry-out: rounding can promote a mantissa from `2^outBits - 1` to
 *      `2^outBits`, bumping the exponent. roundMantissa returns the
 *      bumped exp in that case; we re-check the overflow guard with the
 *      post-round exp.
 *
 *   5. Assemble the double.
 *      Pack `sign + biased_exp + 52-bit-fraction` into a 64-bit pattern
 *      via a shared ArrayBuffer + Float64Array/DataView aliasing trick
 *      (same buffer set_d.ts uses, but in the OPPOSITE direction:
 *      write the bits via DataView.setBigUint64, read the double from
 *      Float64Array[0]).
 *
 * Symmetry with set_d
 * -------------------
 *
 * set_d.ts goes: double → bits → (sign, exp_biased, mant52) → (sign, exp_mpfr,
 *   mant53) → round prec→53 → MPFR.
 * get_d.ts goes: MPFR → round prec→53 → (sign, exp_mpfr, mant53) →
 *   (sign, exp_biased, mant52) → bits → double.
 *
 * The same `roundMantissa` primitive serves both; the same F64 aliasing
 * buffer serves both. We duplicate the buffer + extractDouble inverse
 * (`assembleDouble`) here rather than centralising — set_d.ts deliberately
 * keeps these helpers local until a third consumer forces a substrate
 * extraction (per the note in set_d.ts on `mpfr_round_raw`).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/get_d.c L34–L132 — C reference. mpfr_round_raw_4 + scale2.
 *   - src/ops/set_d.ts — the inverse direction; roundMantissa logic
 *     mirrored.
 *   - src/core.ts §"validate" — input invariants on the MPFR arg.
 *   - eval/functions/mpfr_get_d/spec.json — class, signature, divergence.
 *   - CLAUDE.md Law 4 — schema imports; "Hallucination-risk callouts" —
 *     signed zero, rounding-mode count.
 */

import type { MPFR, RoundingMode, Sign, Ternary } from '../core.ts';
import { MPFRError, validate } from '../core.ts';

// ---------------------------------------------------------------------------
// IEEE 754 bit-pattern assembly
// ---------------------------------------------------------------------------

/**
 * Shared 8-byte buffer for bits-to-double conversion. Aliased by both
 * `Float64Array` and `DataView` so writes through one are observable
 * through the other. Module-level — one allocation per process, no
 * re-entrancy concerns under Bun's single-threaded worker.
 *
 * Endianness: we always write big-endian via `DataView.setBigUint64(0,
 * bits)` (the JS default) and read the double from `Float64Array[0]`,
 * which interprets the buffer in the host's native endianness. On the
 * little-endian hosts we support (x86_64, aarch64) the two layouts
 * differ, so we instead write LE bytes explicitly with the
 * `littleEndian = true` flag — matching set_d.ts's read side exactly.
 */
const F64_BUF = new ArrayBuffer(8);
const F64_VIEW = new Float64Array(F64_BUF);
const U32_VIEW = new DataView(F64_BUF);

/**
 * Pack a sign / 11-bit biased exponent / 52-bit mantissa fraction into a
 * 64-bit IEEE 754 binary64 bit pattern and return the resulting double.
 *
 * Pre-conditions:
 *   - `sign` is 1 or -1 (mapped to bit 63 = 0 or 1).
 *   - `biasedExp` is in `[0, 2047]` (0 = subnormal/zero; 2047 reserved
 *     for Inf/NaN but we don't construct those through this helper —
 *     specials route to direct `Number.POSITIVE_INFINITY` etc.).
 *   - `mant52` is in `[0, 2^52)`. For normals this is the explicit
 *     fraction (the implicit-1 lives in the biased exponent != 0
 *     encoding). For subnormals (biasedExp == 0) this is the entire
 *     significand.
 */
function assembleDouble(sign: Sign, biasedExp: number, mant52: bigint): number {
  // Build the 64-bit pattern. The high 32 bits hold sign + 11-bit exp +
  // top 20 mantissa bits; the low 32 bits hold the bottom 32 mantissa
  // bits. We assemble via bigint shifts then split into two Uint32s.
  const signBit = sign === -1 ? 1n << 63n : 0n;
  const expBits = BigInt(biasedExp & 0x7ff) << 52n;
  const bits = signBit | expBits | (mant52 & ((1n << 52n) - 1n));
  // Split into low/high 32-bit halves and write little-endian, matching
  // the read pattern in set_d.ts's extractDouble.
  const lo = Number(bits & 0xffffffffn);
  const hi = Number((bits >> 32n) & 0xffffffffn);
  U32_VIEW.setUint32(0, lo, /* littleEndian */ true);
  U32_VIEW.setUint32(4, hi, /* littleEndian */ true);
  const v = F64_VIEW[0];
  // `F64_VIEW[0]` is typed as `number | undefined` in lib-dom (sparse
  // array semantics) but a fixed-length TypedArray never returns
  // undefined at an in-range index. The assertion narrows TS.
  if (v === undefined) {
    throw new MPFRError('EDOMAIN', 'assembleDouble: F64_VIEW index 0 is undefined (unreachable)');
  }
  return v;
}

// ---------------------------------------------------------------------------
// Rounding
// ---------------------------------------------------------------------------

/**
 * Round-step result. Same shape set_d.ts uses; duplicated locally because
 * the rounding logic isn't yet extracted to a substrate primitive (see
 * the same note in set_d.ts).
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
 *
 * The algorithm mirrors set_d.ts's roundMantissa one-for-one; see that
 * file for full commentary on the RNDN tie-to-even branch and the
 * carry-out handling. The only direction-specific change here is that
 * the call site is now precision-shrinking on a downstream path (MPFR
 * value → double's 53 or fewer bits) rather than precision-shrinking on
 * an upstream path (double's 53 bits → user's narrow prec); the
 * primitive itself is direction-agnostic.
 *
 * Ref: src/ops/set_d.ts § roundMantissa for full commentary.
 * Ref: mpfr/src/round_raw_generic.c — the canonical primitive.
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
  const trunc = srcMant >> k;
  const droppedMask = (1n << k) - 1n;
  const dropped = srcMant & droppedMask;

  if (dropped === 0n) {
    return { mant: trunc, exp: srcExp, ternary: 0 };
  }

  let increment: boolean;
  switch (rnd) {
    case 'RNDZ':
      increment = false;
      break;
    case 'RNDA':
      increment = true;
      break;
    case 'RNDD':
      increment = sign === -1;
      break;
    case 'RNDU':
      increment = sign === 1;
      break;
    case 'RNDN': {
      const half = 1n << (k - 1n);
      if (dropped > half) {
        increment = true;
      } else if (dropped < half) {
        increment = false;
      } else {
        increment = (trunc & 1n) === 1n;
      }
      break;
    }
    default: {
      const _exhaustive: never = rnd;
      void _exhaustive;
      throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
    }
  }

  if (!increment) {
    return {
      mant: trunc,
      exp: srcExp,
      ternary: sign === 1 ? -1 : 1,
    };
  }

  const incremented = trunc + 1n;
  const upperBound = 1n << outPrec;
  if (incremented === upperBound) {
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
// Validation
// ---------------------------------------------------------------------------

/**
 * Validate the rounding mode at the public boundary. Throws
 * {@link MPFRError} with code `EROUND` on an unknown string. The MPFR
 * value is validated separately via {@link validate} from src/core.ts.
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
// Overflow / underflow handling
// ---------------------------------------------------------------------------

/**
 * The largest finite binary64 in magnitude. 2^1024 * (1 - 2^-53) =
 * (2^53 - 1) * 2^971. Cached for the overflow branch.
 */
const DBL_MAX_VAL = Number.MAX_VALUE; // 1.7976931348623157e+308

/**
 * The smallest positive subnormal binary64. 2^-1074. Cached for the
 * underflow branch.
 */
const DBL_TRUE_MIN_VAL = Number.MIN_VALUE; // 5e-324

/**
 * Choose the overflow target. `exp > 1024` is the overflow trigger;
 * the choice between ±DBL_MAX and ±Infinity is rounding-mode-dependent
 * and matches mpfr/src/get_d.c L83–L90 exactly.
 *
 *   positive:  RNDZ, RNDD          → +DBL_MAX
 *              RNDN, RNDU, RNDA    → +Infinity
 *   negative:  RNDZ, RNDU          → -DBL_MAX
 *              RNDN, RNDD, RNDA    → -Infinity
 */
function overflowTarget(sign: Sign, rnd: RoundingMode): number {
  if (sign === 1) {
    if (rnd === 'RNDZ' || rnd === 'RNDD') return DBL_MAX_VAL;
    return Number.POSITIVE_INFINITY;
  }
  if (rnd === 'RNDZ' || rnd === 'RNDU') return -DBL_MAX_VAL;
  return Number.NEGATIVE_INFINITY;
}

/**
 * Compare |value| against 2^-1075 (half the smallest subnormal) for the
 * RNDN underflow tie-breaker.
 *
 * In the schema, |value| = mant * 2^(exp - prec). The threshold
 * 2^-1075 = 1 * 2^(-1075). We compare without going through floating
 * point: |value| > 2^-1075 iff mant > 2^(prec + exp - (-1075) - 1) -
 * actually let's reason directly:
 *
 *   mant * 2^(exp - prec)  >  2^-1075
 *   mant                   >  2^(-1075 - exp + prec)
 *   mant                   >  2^(prec - exp - 1075)
 *
 * If `prec - exp - 1075 < 0`, the RHS is < 1 ≤ mant, so the LHS > RHS
 * always (the inequality holds). If `prec - exp - 1075 >= 0`, compare
 * directly.
 *
 * For the boundary case exp = -1074 (just below the underflow threshold
 * exp <= -1074 in the schema; recall the underflow trigger in the C
 * reference is `e < -1073` in MPFR-convention exponent — i.e. exp <=
 * -1074 in our schema since C uses [1/2, 1) and we use [2^(exp-1),
 * 2^exp)... wait):
 *
 * Let's normalise. mpfr/src/get_d.c uses MPFR_GET_EXP which returns the
 * canonical MPFR exponent where the value's magnitude lies in
 * [2^(e-1), 2^e). That's exactly our schema. So "the smallest subnormal
 * is 2^-1074 with MPFR exp = -1073" (since 2^-1074 ∈ [2^-1074, 2^-1073),
 * giving e = -1073). The C overflow guard `e < -1073` (i.e. our exp <
 * -1073, i.e. our exp <= -1074) is the magnitude-strictly-below-2^-1074
 * case — the "no double survives the underflow" case.
 *
 * Then the tie-breaker for RNDN at that case compares against 2^-1075 =
 * half the smallest subnormal (the half-way point between 0 and the
 * smallest representable positive double).
 *
 * Returns:
 *   - +1 if |value| > 2^-1075
 *   - 0 if |value| == 2^-1075
 *   - -1 if |value| < 2^-1075
 */
function compareMagToHalfMinSubnormal(x: MPFR): -1 | 0 | 1 {
  // |value| = mant * 2^(exp - prec). Compare against 2^-1075.
  // mant * 2^(exp - prec) <=> 2^-1075
  // ⇔  mant <=> 2^(prec - exp - 1075)
  const threshExp = x.prec - x.exp - 1075n;
  if (threshExp < 0n) {
    // RHS < 1 ≤ mant (mant ≥ 2^(prec-1) ≥ 1 for prec ≥ 1).
    return 1;
  }
  const thresh = 1n << threshExp;
  if (x.mant > thresh) return 1;
  if (x.mant < thresh) return -1;
  return 0;
}

/**
 * Choose the underflow target for `exp < -1073` (magnitude strictly
 * below 2^-1074). Mirrors mpfr/src/get_d.c L66–L81.
 *
 *   positive:  RNDU                                                     → +2^-1074
 *              RNDN if |x| > 2^-1075 (strictly more than half-way up)   → +2^-1074
 *              otherwise                                                 → +0
 *   negative:  RNDD                                                     → -2^-1074
 *              RNDN if |x| > 2^-1075                                     → -2^-1074
 *              otherwise                                                 → -0
 *   RNDA is folded to RNDU/RNDD by sign before this function is called.
 */
function underflowTarget(x: MPFR, sign: Sign, rnd: RoundingMode): number {
  if (sign === 1) {
    if (rnd === 'RNDU') return DBL_TRUE_MIN_VAL;
    if (rnd === 'RNDN' && compareMagToHalfMinSubnormal(x) > 0) return DBL_TRUE_MIN_VAL;
    return 0;
  }
  if (rnd === 'RNDD') return -DBL_TRUE_MIN_VAL;
  if (rnd === 'RNDN' && compareMagToHalfMinSubnormal(x) > 0) return -DBL_TRUE_MIN_VAL;
  // -0 explicitly. `0 * -1` would give 0 (not -0); `-0` literal in source
  // is the explicit IEEE 754 minus-zero.
  return -0;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Convert an {@link MPFR} value to the IEEE 754 binary64 closest to it
 * under `rnd`. Inverse of `mpfr_set_d`.
 *
 * @mpfrName mpfr_get_d
 *
 * @param x    the MPFR value to convert. Must pass {@link validate}.
 *             Signed zero is preserved; NaN converts to NaN; ±Inf to
 *             ±Infinity.
 * @param rnd  one of the five rounding modes in {@link RoundingMode}.
 *
 * @returns    the closest binary64 to `x`. NaN if `x` is NaN; ±Infinity
 *             if `x` is ±Inf or overflows the double range under `rnd`;
 *             ±0 if `x` is ±0 or underflows toward zero under `rnd`;
 *             a finite double otherwise.
 *
 * @throws {MPFRError} `EROUND` on unknown rounding mode; `EPREC` if the
 *                    input fails structural validation.
 *
 * @example
 *   mpfr_get_d(setD(0, 53n, 'RNDN').value, 'RNDN');   //  0
 *   mpfr_get_d(setD(-0, 53n, 'RNDN').value, 'RNDN');  // -0
 *   mpfr_get_d(NAN_VALUE, 'RNDN');                    //  NaN
 *   mpfr_get_d(posInf(53n), 'RNDN');                  // +Infinity
 *   mpfr_get_d(setD(3.14, 200n, 'RNDN').value, 'RNDN'); // 3.14 (round 200 → 53)
 */
export function mpfr_get_d(x: MPFR, rnd: RoundingMode): number {
  validateRnd(rnd);
  // Validate the input structurally. This catches malformed MPFR values
  // that JSON-decoded callers (the grader) might pass; in-library callers
  // produce values that pass validate by construction.
  validate(x);

  // --- Specials ------------------------------------------------------------
  if (x.kind === 'nan') {
    return Number.NaN;
  }
  if (x.kind === 'inf') {
    return x.sign === 1 ? Number.POSITIVE_INFINITY : Number.NEGATIVE_INFINITY;
  }
  if (x.kind === 'zero') {
    // Signed zero is observable; preserve x.sign.
    return x.sign === 1 ? 0 : -0;
  }

  // --- Normal value --------------------------------------------------------
  // Fold RNDA to RNDU (positive) / RNDD (negative). Mirrors mpfr/src/get_d.c
  // L61–L62. After this fold, the body only needs to handle RNDN/RNDZ/RNDU/
  // RNDD, simplifying the overflow/underflow tables.
  let effectiveRnd: RoundingMode = rnd;
  if (rnd === 'RNDA') {
    effectiveRnd = x.sign === -1 ? 'RNDD' : 'RNDU';
  }

  const sign: Sign = x.sign;
  const exp = x.exp;
  const prec = x.prec;
  const mant = x.mant;

  // --- Overflow: |value| above DBL_MAX -------------------------------------
  // The largest finite binary64 is 2^1024 * (1 - 2^-53), with MPFR exp =
  // 1024 (since |DBL_MAX| < 2^1024). Any value with exp > 1024 strictly
  // exceeds the double range. Values with exp == 1024 may or may not
  // overflow depending on the mantissa; we round first then re-check.
  if (exp > 1024n) {
    return overflowTarget(sign, effectiveRnd);
  }

  // --- Underflow: |value| below 2^-1074 ------------------------------------
  // The smallest positive double is 2^-1074, with MPFR exp = -1073
  // (magnitude in [2^-1074, 2^-1073)). Values with exp < -1073 (i.e. <=
  // -1074 in our schema) are strictly less than 2^-1074 in magnitude.
  // Note the C reference's `e < -1073` uses the same MPFR convention.
  if (exp < -1073n) {
    return underflowTarget(x, sign, effectiveRnd);
  }

  // --- In-range: -1073 <= exp <= 1024 --------------------------------------
  // Determine target mantissa width. Subnormal-target range is exp in
  // [-1073, -1021]; biased exponent will be 0 and mantissa width is
  // < 53 bits. Normal-target range is exp in [-1021, 1024]; biased
  // exponent in [1, 2046] (modulo carry-out) and mantissa width = 53.
  //
  // The exp == -1021 boundary belongs to the normal-target range (the
  // smallest normal double is DBL_MIN = 2^-1022 with MPFR exp = -1021,
  // since 2^-1022 ∈ [2^-1022, 2^-1021)).
  let outBits: bigint;
  let isSubnormalTarget: boolean;
  if (exp >= -1021n) {
    // Normal-target range: |value| ∈ [DBL_MIN, ∞). 53 bits of mantissa,
    // biased exponent in [1, 2046] (modulo post-round overflow check).
    outBits = 53n;
    isSubnormalTarget = false;
  } else {
    // Subnormal-target range: exp ∈ [-1073, -1022]. The IEEE 754 binary64
    // subnormal grid has step 2^-1074, with biased exponent 0 and a
    // significand of <= 52 bits depending on the magnitude.
    //
    // The number of significant bits in the output mantissa is
    // `1074 + exp`. Mirrors mpfr/src/get_d.c L101 (`nbits += 1021 + e`)
    // with C's `e` and our `exp` using the same MPFR convention (both
    // [2^(e-1), 2^e) magnitude). So nbits = 53 + 1021 + e_C = 1074 +
    // exp.
    //
    // Range check (matches C's MPFR_ASSERTD at L102: 1 <= nbits < 53):
    //   exp = -1073 → outBits = 1  (smallest subnormal: 2^-1074)
    //   exp = -1072 → outBits = 2
    //   ...
    //   exp = -1022 → outBits = 52 (largest subnormal < DBL_MIN)
    //   exp = -1021 was routed to the normal branch above (outBits=53).
    outBits = 1074n + exp;
    isSubnormalTarget = true;
  }

  // --- Round the source mantissa to outBits --------------------------------
  let rounded: RoundedMantissa;
  if (prec <= outBits) {
    // Lossless: pad with zeros. The padded mantissa is mant << (outBits -
    // prec), and the exponent is unchanged.
    const pad = outBits - prec;
    rounded = { mant: mant << pad, exp, ternary: 0 };
  } else {
    rounded = roundMantissa(mant, prec, exp, outBits, sign, effectiveRnd);
  }

  // --- Post-round re-check for overflow ------------------------------------
  // Rounding can bump exp by 1 (carry-out). Re-check whether we just
  // crossed into the overflow region.
  if (rounded.exp > 1024n) {
    return overflowTarget(sign, effectiveRnd);
  }

  // --- Subnormal-target assembly -------------------------------------------
  // For subnormal output (exp < -1021, including the carry-promoted case
  // where rounded.exp might now be -1021 — see below): biased exponent
  // is 0, and the 52-bit mantissa fraction holds the entire significand
  // (no implicit-1).
  //
  // Carry-out edge case: if we entered with isSubnormalTarget but the
  // round bumped rounded.exp such that rounded.exp >= -1021, the value
  // crossed the subnormal/normal boundary up to DBL_MIN. The result is
  // the normal DBL_MIN with biased exp = 1 and mant52 = 0. We detect
  // this and re-route.
  if (isSubnormalTarget && rounded.exp >= -1021n) {
    // Carry promoted the value to the smallest normal. By construction
    // the rounded mantissa is 2^outBits (pre-shift to 2^(outBits-1) by
    // roundMantissa), giving exactly DBL_MIN = 2^-1022.
    return assembleDouble(sign, 1, 0n);
  }

  if (isSubnormalTarget) {
    // Build subnormal: biased exp = 0, mant52 = the entire 52-bit
    // significand (no implicit-1). The IEEE 754 subnormal value is
    // mant52 * 2^-1074.
    //
    // After rounding, rounded.mant is MSB-aligned to outBits bits
    // (rounded.mant ∈ [2^(outBits-1), 2^outBits)) and represents the
    // value rounded.mant * 2^(rounded.exp - outBits).
    //
    // We need mant52 with mant52 * 2^-1074 == rounded.mant *
    // 2^(rounded.exp - outBits). Solving:
    //
    //   mant52 = rounded.mant * 2^(rounded.exp - outBits + 1074)
    //
    // Substituting outBits = 1074 + exp (the pre-round value): if
    // rounding did NOT carry (rounded.exp === exp), the shift exponent
    // is exp - (1074 + exp) + 1074 = 0, so mant52 = rounded.mant
    // directly. If rounding carried (rounded.exp === exp + 1), the
    // shift is +1 and mant52 = 2 * rounded.mant — but we caught the
    // subnormal-to-normal carry case above (rounded.exp >= -1021), so
    // a carry inside the subnormal range stays subnormal with the
    // promoted mantissa fitting in 52 bits by the carry's structure.
    //
    // Sanity-check the formula on value 3*2^-1074 at prec=53, exp=-1072:
    //   outBits = 1074 + (-1072) = 2; k = 51; trunc = 3, dropped = 0.
    //   rounded = {mant: 3, exp: -1072, ternary: 0}.
    //   shift = -1072 - 2 + 1074 = 0.
    //   mant52 = 3 << 0 = 3.
    //   assemble: biased exp 0, mant52 = 3 → value 3 * 2^-1074. ✓
    const shift = rounded.exp - outBits + 1074n;
    const mant52 =
      shift >= 0n ? rounded.mant << shift : rounded.mant >> -shift;
    return assembleDouble(sign, 0, mant52);
  }

  // --- Normal-target assembly ----------------------------------------------
  // rounded.mant is MSB-aligned to 53 bits, so 2^52 ≤ rounded.mant < 2^53.
  // The 52-bit mantissa fraction is rounded.mant minus the implicit-1
  // (bit 52). Biased exponent = rounded.exp + 1022.
  //
  // Pre-overflow check: rounded.exp must be in [-1021, 1024]. The carry
  // case where rounded.exp == 1025 was caught by the post-round overflow
  // re-check above. The subnormal-promoted case where exp crossed -1021
  // from below was handled in the subnormal branch.
  const biasedExp = Number(rounded.exp + 1022n);
  const mant52 = rounded.mant - (1n << 52n);
  return assembleDouble(sign, biasedExp, mant52);
}
