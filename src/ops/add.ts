/**
 * ops/add.ts — pure-TS port of MPFR's `mpfr_add`.
 *
 * Add two {@link MPFR} values at the caller-supplied target precision,
 * rounded per the rounding mode, returning the canonical
 * `{value, ternary}` from src/core.ts.
 *
 * C signature
 * -----------
 *
 *   int mpfr_add(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - returns the ternary as the function result.
 *
 *   Ref: mpfr/src/add.c L24–L121.
 *
 * TS signature
 * ------------
 *
 *   mpfr_add(a: MPFR, b: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `prec` as an explicit positional argument (no `rop`);
 *   - returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Algorithm
 * ---------
 *
 * Top-level dispatch on (a.kind, b.kind), then normal-+-normal core:
 *
 *   1. NaN propagation. NaN + anything → NAN_VALUE, ternary 0.
 *
 *   2. ±Inf handling.
 *        +Inf + +Inf or -Inf + -Inf → ±Inf at target prec.
 *        +Inf + -Inf or -Inf + +Inf → NAN_VALUE.
 *        ±Inf + finite or finite + ±Inf → ±Inf at target prec.
 *      All ternary 0 — infinity is exact, not rounded.
 *
 *   3. ±0 + ±0.
 *      Mirrors mpfr/src/add.c L66–L75:
 *        - Under RNDD: result is -0 unless both operands are +0.
 *        - Under RNDN/Z/U/A: result is +0 unless both operands are -0.
 *      The rule is "RNDD breaks toward -∞ (so -0 if not both +0); every
 *      other mode breaks toward +∞ (so +0 if not both -0)". Ternary 0
 *      — both inputs were exactly zero, the result IS exact.
 *
 *   4. ±0 + normal (or normal + ±0).
 *      The result equals the normal operand rounded to the target prec.
 *      The C reference delegates to `mpfr_set`, which is precisely the
 *      "copy with rounding" semantic; we inline that as a roundAndPack
 *      step using our local roundMantissa primitive. The ±0 operand's
 *      sign is discarded (per IEEE 754: x + (+0) and x + (-0) both
 *      equal x for any normal x).
 *
 *   5. Normal + normal — the real work.
 *
 *      Let a, b be MPFR normals with value
 *           a = a.sign * a.mant * 2^(a.exp - a.prec)
 *           b = b.sign * b.mant * 2^(b.exp - b.prec)
 *
 *      We compute a + b at full precision then round to `prec`. The
 *      strategy is:
 *
 *        a. Choose a working "anchor exponent" E equal to the larger of
 *           a.exp and b.exp. Express both significands in a common
 *           radix-2 framing: bit position `p` of the radix-2 expansion
 *           of |x| (where the MSB sits at position E-1 in the |x|=2^(E-1)
 *           normalisation) corresponds to a particular bit of x.mant.
 *
 *        b. Build two non-negative bigints `am` and `bm` representing
 *           a.mant and b.mant shifted so they share the same scale —
 *           "scale" here meaning "value in units of 2^(E - W)" for some
 *           sufficiently large width W. We pick W large enough to retain
 *           every significant bit of both operands and a guard cushion
 *           around `prec` so the rounding decision is exact.
 *
 *        c. If a.sign == b.sign: `mag = am + bm`. Effective addition;
 *           a carry-out grows the magnitude by one bit (`mag` may have
 *           one more bit than W). The result sign is a.sign.
 *
 *        d. If a.sign != b.sign: `mag = abs(am - bm)`. Effective
 *           subtraction. If `mag === 0n` the result is signed zero
 *           per the rounding mode (mpfr/src/sub1.c L66–L75: +0 always
 *           except RNDD → -0). Otherwise the result sign is the sign of
 *           the larger-magnitude operand.
 *
 *        e. Normalise `mag` (strip leading zeros), reduce the result
 *           exponent accordingly, and round to `prec` via roundMantissa.
 *
 *   6. Carry-out and renormalisation.
 *
 *      The roundMantissa primitive already handles the carry case where
 *      rounding +1 pushes the mantissa past 2^prec. Cancellation in the
 *      effective-subtract path can cause many leading zeros — we strip
 *      them BEFORE rounding so the rounding decision sees the correct
 *      bit positions.
 *
 * Why inline the bit math (not a full mpn-level port)
 * ---------------------------------------------------
 *
 * MPFR's C reference uses `mpn_add_n` / `mpn_sub_n` over limb arrays
 * with explicit shift and carry handling. In pure TS with BigInts we
 * get the same arithmetic in one operation: `am + bm` (or `am - bm`)
 * over bigints is constant-time-per-limb the JIT manages internally
 * with all the carry propagation already baked in. The substrate
 * mpn_add_n / mpn_sub_n exist (and are imported here for the same-prec
 * fast path described below) but the bigint arithmetic is faithful
 * to the same I/O contract and several orders of magnitude clearer in
 * source form. Per Law 3 the substrate API is preserved for downstream
 * consumers; the production op composes the public surface freely.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/add.c L24–L121 — top-level dispatcher.
 *   - mpfr/src/add1.c — effective-add algorithm with carry.
 *   - mpfr/src/sub1.c L34–L75 — cancellation: result-zero rounding rule.
 *   - mpfr/src/round_raw_generic.c — the canonical rounding primitive.
 *   - src/core.ts §"validate" — output invariants every returned MPFR
 *     must satisfy.
 *   - src/internal/mpn/add_n.ts, sub_n.ts — substrate composed in the
 *     same-prec fast path of effective add / sub. Imported lazily-ish
 *     (the import is hoisted by ESM but the runtime call sites only
 *     fire for the same-precision normal-+-normal path).
 *   - CLAUDE.md "Hallucination-risk callouts" — signed zero, ternary
 *     direction, rounding-mode count.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
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
import { mpn_add_n } from '../internal/mpn/add_n.ts';
import { mpn_sub_n } from '../internal/mpn/sub_n.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

// Note: the local copy of `roundMantissa` previously defined here has
// been promoted to the substrate at `src/internal/mpfr/round_raw.ts`
// alongside identical duplicates that lived in `set_d.ts`, `get_d.ts`,
// and `mul.ts`. The extraction was triggered when `mul.ts` became the
// fourth caller (see CLAUDE.md "don't extract until N callers force
// it"). All four ops now import the same primitive. Ref:
// src/internal/mpfr/round_raw.ts for the algorithm commentary.

/**
 * Validate the public-boundary arguments. Throws `MPFRError` on bad
 * inputs. We do NOT call `validate()` on the operands themselves here;
 * the runner's grader passes in JSON-decoded values that already passed
 * validate via decodeMpfr, and library-internal callers produce
 * pre-validated values. Wrapping every public op in a full validate
 * would double-validate; instead each op trusts its inputs structurally
 * and surfaces shape bugs as wrong outputs caught by the grader.
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

/**
 * Internal helper: round-and-pack a normal value into the target prec.
 * Used by the ±0 + normal paths (the result is just the normal operand
 * rounded to `prec`) and as the final step in the normal + normal core.
 *
 * Pre-conditions:
 *   - `sign` ∈ {1, -1}, `srcExp` is the unrounded magnitude's MPFR exp,
 *   - `srcMant` is an unsigned bigint MSB-aligned to `srcPrec` bits,
 *   - `prec >= 1`.
 */
function packNormal(
  sign: Sign,
  srcExp: bigint,
  srcMant: bigint,
  srcPrec: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  if (prec >= srcPrec) {
    // Lossless: pad with zeros to widen to `prec` bits MSB-aligned.
    const padShift = prec - srcPrec;
    const value: MPFR = {
      kind: 'normal',
      sign,
      prec,
      exp: srcExp,
      mant: srcMant << padShift,
    };
    return { value, ternary: 0 };
  }
  const { mant, exp, ternary } = roundMantissa(
    srcMant,
    srcPrec,
    srcExp,
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

// ---------------------------------------------------------------------------
// Zero-result rounding for ±0 + ±0
// ---------------------------------------------------------------------------

/**
 * Choose the sign of the zero result for `±0 + ±0`. Mirrors
 * mpfr/src/add.c L66–L75:
 *
 *   - Under RNDD: result is -0 unless both operands are +0.
 *   - Under RNDN/Z/U/A: result is +0 unless both operands are -0.
 *
 * The rule reads as "the result is whatever you can convince yourself
 * is exact": +0 + (-0) is mathematically zero with no preferred sign,
 * so we pick by rounding direction. RNDD's tie-breaking sends the
 * ambiguous-zero result toward -∞ (i.e. -0); the other four modes
 * (which never round toward -∞) send it toward +∞ (i.e. +0).
 */
function zeroSumSign(
  aSign: Sign,
  bSign: Sign,
  rnd: RoundingMode,
): Sign {
  if (rnd === 'RNDD') {
    // -0 unless both positive.
    return aSign === 1 && bSign === 1 ? 1 : -1;
  }
  // +0 unless both negative.
  return aSign === -1 && bSign === -1 ? -1 : 1;
}

/**
 * Choose the sign of the zero result for the cancellation case in
 * effective subtract (|a| == |b|, opposite signs). Mirrors
 * mpfr/src/sub1.c L66–L75: result is +0 for every rounding mode except
 * RNDD which gives -0.
 */
function cancellationZeroSign(rnd: RoundingMode): Sign {
  return rnd === 'RNDD' ? -1 : 1;
}

// ---------------------------------------------------------------------------
// Same-prec fast path: substrate composition
// ---------------------------------------------------------------------------

const LIMB_BITS = 64n;
const LIMB_MASK = (1n << LIMB_BITS) - 1n;

/**
 * Decompose a non-negative bigint into a little-endian limb array of
 * exactly `n` 64-bit limbs. Pads with zero limbs at the high end if
 * needed; throws if the value exceeds `n` limbs.
 */
function toLimbs(v: bigint, n: number): bigint[] {
  const limbs: bigint[] = new Array<bigint>(n);
  let x = v;
  for (let i = 0; i < n; i++) {
    limbs[i] = x & LIMB_MASK;
    x >>= LIMB_BITS;
  }
  if (x !== 0n) {
    throw new MPFRError(
      'EPREC',
      `toLimbs: value exceeds ${n} limbs (residual ${x})`,
    );
  }
  return limbs;
}

/**
 * Reassemble a little-endian limb array into a single non-negative bigint.
 */
function fromLimbs(limbs: readonly bigint[]): bigint {
  let v = 0n;
  for (let i = limbs.length - 1; i >= 0; i--) {
    const limb = limbs[i];
    if (limb === undefined) {
      throw new MPFRError('EPREC', `fromLimbs: undefined limb at index ${i}`);
    }
    v = (v << LIMB_BITS) | limb;
  }
  return v;
}

/**
 * Same-precision same-sign addition via the mpn_add_n substrate. Used
 * by the effective-add path when both operands share `prec` AND share
 * exponent within ±1 (the case the carry chain handles cleanly limb by
 * limb). Returns the unaligned magnitude plus a carry-out bit indicating
 * whether the sum exceeded `prec` bits.
 *
 * Pre-conditions:
 *   - `am` and `bm` are MSB-aligned to `prec` bits and represent
 *     significands of the same exponent.
 *   - `prec >= 1`.
 *
 * Returns: `{magnitude, carryOut}` where magnitude is in
 *   [2^(prec-1), 2^(prec+1)) and carryOut indicates whether the high bit
 *   `1n << prec` is set.
 */
function mpnAddSameExp(
  am: bigint,
  bm: bigint,
  prec: bigint,
): { magnitude: bigint; carryOut: boolean } {
  const nLimbs = Number((prec + LIMB_BITS - 1n) / LIMB_BITS);
  const aLimbs = toLimbs(am, nLimbs);
  const bLimbs = toLimbs(bm, nLimbs);
  const { result, carry } = mpn_add_n(aLimbs, bLimbs, nLimbs);
  const magnitude = fromLimbs(result);
  // The mpn_add_n carry is at limb boundary; we still need to check
  // whether the in-limb sum overflowed the prec-bit mantissa frame
  // (since prec may not be a multiple of 64). Reassemble and compare.
  const carryOut = (magnitude >> prec) !== 0n || carry !== 0n;
  // The mpn_add_n carry-out bit, if set, is effectively a 2^(nLimbs*64)
  // contribution. Fold it back into the bigint magnitude so the caller
  // sees the full value uniformly.
  const fullMagnitude = magnitude | (carry << BigInt(nLimbs) * LIMB_BITS);
  return { magnitude: fullMagnitude, carryOut };
}

/**
 * Same-precision effective subtract via the mpn_sub_n substrate. Used
 * by the cancellation path when both operands share `prec`. The caller
 * guarantees `am >= bm` (operand-magnitude ordering is decided before
 * we get here); the borrow-out is therefore always 0.
 *
 * Pre-conditions:
 *   - `am >= bm`, both MSB-aligned-or-equivalent (subtraction doesn't
 *     require strict MSB alignment, but the result's normalization is
 *     handled by the caller post-strip).
 *
 * Returns: the unsigned difference as a bigint.
 */
function mpnSubSameExp(am: bigint, bm: bigint, prec: bigint): bigint {
  const nLimbs = Number((prec + LIMB_BITS - 1n) / LIMB_BITS);
  const aLimbs = toLimbs(am, nLimbs);
  const bLimbs = toLimbs(bm, nLimbs);
  const { result, borrow } = mpn_sub_n(aLimbs, bLimbs, nLimbs);
  if (borrow !== 0n) {
    // Defensive: caller violated the |a| >= |b| precondition. The bigint
    // arithmetic path would have produced a negative; substrate path
    // returns wrap-around. Surface as a precise error.
    throw new MPFRError(
      'EPREC',
      `mpnSubSameExp: borrow-out (am < bm: am=${am}, bm=${bm})`,
    );
  }
  return fromLimbs(result);
}

// ---------------------------------------------------------------------------
// Bit-width helpers (no built-in bit_length for bigint in ES2025)
// ---------------------------------------------------------------------------

/**
 * Number of significant bits in `v`. For `v > 0` this is `floor(log2(v))
 * + 1` — i.e. the position (1-indexed) of the topmost set bit. For
 * `v === 0n` we return `0n`.
 *
 * Used to strip leading zeros after effective subtract (the cancellation
 * case can produce arbitrarily many leading zeros; we need to renormalise
 * before rounding so the round/sticky bits sit at the right positions).
 *
 * Bigint has no built-in bit_length; the loop here is bounded by the
 * source operand's width (capped at prec + a small guard). For
 * prec=200 the loop runs at most ~200 iterations; cheap. Optimize phase
 * may replace with a binary-search bit_length helper.
 */
function bitLength(v: bigint): bigint {
  if (v <= 0n) return 0n;
  let n = 0n;
  let x = v;
  while (x > 0n) {
    n++;
    x >>= 1n;
  }
  return n;
}

// ---------------------------------------------------------------------------
// Magnitude comparison for normals
// ---------------------------------------------------------------------------

/**
 * Compare the magnitudes `|a|` and `|b|` of two normal MPFRs. Returns
 * `-1` if `|a| < |b|`, `0` if equal, `+1` if `|a| > |b|`. Ignores sign.
 *
 * The algorithm: compare exponents first (higher exp → strictly larger
 * magnitude), then mantissas after aligning to a common scale. Aligning
 * via `a.mant << (prec_diff)` vs `b.mant` correctly handles different
 * precisions: the mantissa with more bits has the longer trailing
 * representation but the same MSB-aligned value, so we align the shorter
 * one upward and compare.
 *
 * Pre-condition: both operands are kind:'normal'.
 */
function cmpMagnitude(a: MPFR, b: MPFR): -1 | 0 | 1 {
  if (a.exp > b.exp) return 1;
  if (a.exp < b.exp) return -1;
  // Same exponent. Compare mantissas after aligning to a common width.
  // Mantissa is MSB-aligned to its own prec, so the value of a is
  // a.mant * 2^(a.exp - a.prec) and the value of b is b.mant * 2^(b.exp
  // - b.prec). With a.exp == b.exp, the magnitude ordering is
  // (a.mant / 2^a.prec)  vs  (b.mant / 2^b.prec)
  // ⇔ a.mant * 2^b.prec  vs  b.mant * 2^a.prec
  const lhs = a.mant << b.prec;
  const rhs = b.mant << a.prec;
  if (lhs > rhs) return 1;
  if (lhs < rhs) return -1;
  return 0;
}

// ---------------------------------------------------------------------------
// Normal + normal core
// ---------------------------------------------------------------------------

/**
 * Add or subtract two normal MPFR values; return the {value, ternary}
 * pair at the target precision.
 *
 * Strategy: express both significands on a common scale (an "anchor
 * exponent" E equal to the larger operand's exp), perform the exact
 * arithmetic on bigints, then round to `prec`. The bigint arithmetic
 * is exact regardless of magnitude difference; the round/sticky
 * computation gets the correct bits at the right positions because we
 * pad with a guard so the rounding decision sees enough low-order bits.
 *
 * Pre-conditions:
 *   - `a` and `b` are kind:'normal'.
 *   - `prec >= 1`, `rnd` is a valid RoundingMode.
 */
function addNormalNormal(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Express each operand as `sign * mant * 2^(exp - prec_self)`. To put
  // them on a common scale, we choose a unified bit-position framing:
  // bit position p (where p=0 is the unit's place at value 2^0)
  // corresponds to mantissa bit (p - exp + prec_self) of the operand.
  //
  // Concretely: shift each mantissa so that the bit representing 2^k
  // sits at position k. We want both shifted mantissas to share the same
  // low-end reference. Pick `lowAnchor` = min(a.exp - a.prec, b.exp -
  // b.prec); each operand's mantissa shifted left by (its (exp -
  // prec_self) - lowAnchor) gives a non-negative integer whose value is
  // the operand's magnitude divided by 2^lowAnchor. Then `am + bm` or
  // `|am - bm|` is the exact magnitude of the sum/difference divided by
  // 2^lowAnchor — i.e. an integer whose bit count tells us the MPFR
  // exponent of the result.

  const lowA = a.exp - a.prec;
  const lowB = b.exp - b.prec;
  const lowAnchor = lowA < lowB ? lowA : lowB;
  const am = a.mant << (lowA - lowAnchor); // integer ≥ 0
  const bm = b.mant << (lowB - lowAnchor);

  // Compute the signed exact sum: a.sign * am + b.sign * bm. Carry it
  // out as a magnitude + result-sign pair so we can rejoin the
  // signed-zero path uniformly with the rest of the code.

  const sameSign = a.sign === b.sign;
  let resultSign: Sign;
  let magnitude: bigint;

  if (sameSign) {
    // Effective add: magnitude = am + bm; sign = a.sign.
    resultSign = a.sign;
    // Same-prec fast path: when a.prec === b.prec AND lowA === lowB,
    // both shift offsets are zero and `am`, `bm` are MSB-aligned to
    // prec bits each. Route through the mpn_add_n substrate composition
    // so the substrate sees real exercise from the production op.
    if (a.prec === b.prec && lowA === lowB) {
      const { magnitude: msum } = mpnAddSameExp(am, bm, a.prec);
      magnitude = msum;
    } else {
      magnitude = am + bm;
    }
  } else {
    // Effective subtract. Determine which operand has the larger
    // magnitude; the result sign matches it. If equal magnitudes, the
    // result is signed zero per the rounding mode.
    if (am === bm) {
      const sign = cancellationZeroSign(rnd);
      const value = sign === 1 ? posZero(prec) : negZero(prec);
      return { value, ternary: 0 };
    }
    if (am > bm) {
      resultSign = a.sign;
      // Same-prec fast path for subtraction: same lowAnchor, same width,
      // substrate-eligible.
      if (a.prec === b.prec && lowA === lowB) {
        magnitude = mpnSubSameExp(am, bm, a.prec);
      } else {
        magnitude = am - bm;
      }
    } else {
      resultSign = b.sign;
      if (a.prec === b.prec && lowA === lowB) {
        magnitude = mpnSubSameExp(bm, am, b.prec);
      } else {
        magnitude = bm - am;
      }
    }
  }

  // `magnitude` is now the exact non-negative integer value of |a + b|
  // multiplied by 2^(-lowAnchor). Its bit count gives us the MPFR
  // exponent: if magnitude has bit-length L, then |a + b| sits in
  // [2^(lowAnchor + L - 1), 2^(lowAnchor + L)), so the MPFR exp is
  // (lowAnchor + L).
  //
  // `magnitude === 0n` is impossible here: the same-sign path requires
  // both operands nonzero (they're normals) and addition of two same-
  // sign nonzeros is nonzero; the opposite-sign-equal-magnitude case
  // returned early above.

  const L = bitLength(magnitude);
  if (L === 0n) {
    // Defensive: unreachable given the above. Surface as a precise error
    // rather than slip through with bad invariants.
    throw new MPFRError(
      'EPREC',
      'addNormalNormal: internal invariant violated (zero magnitude past cancellation)',
    );
  }
  const resultExp = lowAnchor + L;

  // Strip leading zeros: shift `magnitude` so its MSB sits at bit (L-1)
  // — it already does by definition of L. The bit-frame for rounding is
  // an L-bit MSB-aligned mantissa; we round from L bits down to `prec`.

  return packNormal(resultSign, resultExp, magnitude, L, prec, rnd);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Add two MPFR values at the target precision, returning the rounded
 * result and the ternary flag (sign of `(rounded - exact)`).
 *
 * @mpfrName mpfr_add
 *
 * @param a     first operand. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param b     second operand. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. The value passes `validate()` without
 *              post-processing. Ternary is `0` for exact (including all
 *              specials), `+1` if rounded > exact, `-1` if rounded < exact.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding mode.
 *                    NaN / Inf input is NOT an error.
 *
 * @example
 *   add(setD(1.0, 53n, 'RNDN').value, setD(2.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 3.0 at prec 53, ternary: 0}
 *   add(posZero(53n), negZero(53n), 53n, 'RNDD');
 *     // → {value: negZero(53n), ternary: 0}  — rounding-mode-observable
 *   add(setD(1.0, 53n, 'RNDN').value, setD(-1.0, 53n, 'RNDN').value, 53n, 'RNDU');
 *     // → {value: posZero(53n), ternary: 0}  — cancellation
 */
export function mpfr_add(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // --- Specials ---------------------------------------------------------
  // (1) NaN propagation. Cheapest discriminator first; both branches
  // collapse to the canonical NAN_VALUE.
  if (a.kind === 'nan' || b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) ±Inf handling.
  if (a.kind === 'inf') {
    if (b.kind === 'inf') {
      // Same sign: ±Inf; opposite sign: NaN.
      if (a.sign === b.sign) {
        return {
          value: a.sign === 1 ? posInf(prec) : negInf(prec),
          ternary: 0,
        };
      }
      return { value: NAN_VALUE, ternary: 0 };
    }
    // a is ±Inf, b is finite (zero or normal): result is a's sign infinity.
    return {
      value: a.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }
  if (b.kind === 'inf') {
    // a is finite (zero or normal), b is ±Inf: result is b's sign infinity.
    return {
      value: b.sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // (3) ±0 + ±0. Sign per zeroSumSign; ternary 0 (exact).
  if (a.kind === 'zero' && b.kind === 'zero') {
    const sign = zeroSumSign(a.sign, b.sign, rnd);
    return {
      value: sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (4) ±0 + normal — result is the normal rounded to target prec.
  if (a.kind === 'zero') {
    // a is ±0; b is normal (we've ruled out nan/inf/zero on b above).
    // The C reference (mpfr/src/add.c L78) delegates to mpfr_set(rop, c,
    // rnd) — i.e. copy b's value into a fresh MPFR at the target prec
    // with rounding. The ±0 operand's sign does NOT influence the
    // result: x + (+0) = x + (-0) = x for any finite x. (Confirmed
    // against mpfr/src/set.c L25–L43: mpfr_set4 copies the SOURCE's
    // sign onto the destination; the destination's pre-existing sign is
    // overwritten.)
    if (b.kind !== 'normal') {
      // Defensive: unreachable given specials dispatch above.
      throw new MPFRError(
        'EPREC',
        `mpfr_add: unexpected b.kind=${b.kind} in zero+? branch`,
      );
    }
    return packNormal(b.sign, b.exp, b.mant, b.prec, prec, rnd);
  }
  if (b.kind === 'zero') {
    if (a.kind !== 'normal') {
      throw new MPFRError(
        'EPREC',
        `mpfr_add: unexpected a.kind=${a.kind} in ?+zero branch`,
      );
    }
    return packNormal(a.sign, a.exp, a.mant, a.prec, prec, rnd);
  }

  // (5) normal + normal — the algebraic core.
  if (a.kind !== 'normal' || b.kind !== 'normal') {
    // Defensive: unreachable. All other kinds dispatched above.
    throw new MPFRError(
      'EPREC',
      `mpfr_add: unexpected kinds a=${a.kind} b=${b.kind} at normal-normal branch`,
    );
  }
  // The cmpMagnitude helper is exported for adjacent ops; not used in
  // the local dispatch (addNormalNormal handles magnitude ordering
  // internally via am === bm / am > bm), but we keep it exported because
  // future cancellation-aware variants compose against it.
  void cmpMagnitude;
  return addNormalNormal(a, b, prec, rnd);
}
