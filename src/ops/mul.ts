/**
 * ops/mul.ts — pure-TS port of MPFR's `mpfr_mul`.
 *
 * Multiply two {@link MPFR} values at the caller-supplied target precision,
 * rounded per the rounding mode, returning the canonical
 * `{value, ternary}` shape from src/core.ts (Law 4).
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - returns the ternary as the function result.
 *
 *   Ref: mpfr/src/mul.c L172–L237 (mpfr_mul) and L34–L168 (mpfr_mul3 —
 *   the underlying algorithm we mirror).
 *
 * TS signature
 * ------------
 *
 *   mpfr_mul(a: MPFR, b: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `prec` as an explicit positional argument (no `rop`);
 *   - returns the immutable {@link Result} from src/core.ts.
 *
 * Algorithm
 * ---------
 *
 * Top-level dispatch on (a.kind, b.kind), then normal*normal core:
 *
 *   1. NaN propagation. NaN * anything → NAN_VALUE, ternary 0.
 *
 *   2. ±Inf handling.
 *      - Inf * Inf  → sign(a.sign*b.sign) * Inf, ternary 0.
 *      - Inf * 0    → NaN (the canonical 0*Inf indeterminate form;
 *                    mpfr/src/mul.c L60–L86 — the inner check
 *                    `MPFR_IS_INF(c) || MPFR_NOTZERO(c)` rejects the
 *                    zero-finite case explicitly).
 *      - Inf * normal → sign-product * Inf, ternary 0.
 *      - 0   * Inf  → NaN (symmetric).
 *      - normal * Inf → sign-product * Inf, ternary 0.
 *
 *   3. ±0 * ±0 → ±0 with sign = a.sign * b.sign (per
 *      mpfr/src/mul.c L88–L94 — MPFR_MULT_SIGN composes the two signs
 *      directly; the result is exact, ternary 0).
 *
 *   4. ±0 * normal (or normal * ±0) → ±0 with sign = a.sign * b.sign.
 *      Same rule as 0*0 — exact, ternary 0. Note the result sign is
 *      NOT the sign of the normal operand; it is the product of both
 *      signs (mpfr/src/mul.c L90 — `MPFR_SET_SIGN(a, sign_product)`).
 *
 *   5. normal * normal — the real work.
 *
 *      Let a, b be MPFR normals with value
 *           a = a.sign * a.mant * 2^(a.exp - a.prec)
 *           b = b.sign * b.mant * 2^(b.exp - b.prec)
 *
 *      The exact product is therefore
 *           a*b = (a.sign * b.sign) * (a.mant * b.mant)
 *                 * 2^(a.exp + b.exp - a.prec - b.prec)
 *
 *      Strategy:
 *        a. resultSign = a.sign * b.sign  (the product-of-signs rule).
 *        b. product = a.mant * b.mant  — a non-negative bigint with
 *           bit-length in {a.prec + b.prec - 1, a.prec + b.prec}.
 *           The bit-length is exactly a.prec + b.prec iff the MSBs of
 *           both operands multiply with carry into the high bit; it is
 *           a.prec + b.prec - 1 otherwise. (Mirrors mpfr/src/mul.c L131
 *           — `if (b1 == 0) mpn_lshift(...)` — which detects the
 *           "one bit short" case via a high-limb MSB check.)
 *        c. resultExp = a.exp + b.exp + (L - (a.prec + b.prec)),
 *           where L = bitLength(product). When L == a.prec+b.prec this
 *           contributes 0; when L == a.prec+b.prec-1 it contributes -1.
 *           The geometric reason: |a*b| is in [2^(resultExp - 1),
 *           2^resultExp), and |a*b| = product * 2^(a.exp - a.prec +
 *           b.exp - b.prec) so resultExp = (lowAnchor) + L, where
 *           lowAnchor = a.exp - a.prec + b.exp - b.prec. Substituting:
 *           resultExp = a.exp + b.exp + L - a.prec - b.prec.
 *        d. Round the (resultSign, resultExp, product, L)-tuple to
 *           prec bits via the same roundMantissa helper add.ts uses.
 *
 *      Ternary direction depends on the result sign: roundMantissa
 *      takes the sign explicitly and computes the correct sign of
 *      (rounded - exact). This is THE single most subtle bit — the
 *      product's sign is the sign of `a.sign * b.sign`, not just
 *      `a.sign` — getting this wrong silently inverts every ternary on
 *      mixed-sign inputs and never affects the value itself, which is
 *      exactly the regression PIL.3 mutation-proves against.
 *
 * Why no substrate composition
 * ----------------------------
 *
 * `mpfr_add` composes `mpn_add_n` / `mpn_sub_n` because those substrate
 * functions exist (they are pilot functions 1 and 2). There is no
 * `mpn_mul.ts` substrate yet — `mpn_mul` is not in the pilot set — so
 * the bigint product is the faithful substrate-level operation. Per
 * Law 3 the I/O contract matches the C `mpn_mul(tmp, b->mant, bn,
 * c->mant, cn)` call: an exact product of two non-negative integers.
 * When `mpn_mul.ts` is ported later we may revisit this for a
 * limb-array fast path, but the bigint primitive is correct and clear.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/mul.c L34–L168 — mpfr_mul3, the general algorithm.
 *   - mpfr/src/mul.c L52–L94  — specials dispatch.
 *   - mpfr/src/mul.c L96–L168 — normal*normal: product, b1 carry,
 *     round_raw, exp adjustment.
 *   - mpfr/src/mul.c L269–L364 — mpfr_mul_1: the prec≤64 fast path
 *     (one umul_ppmm), as a cross-check of the "b1 ∈ {0,1}" framing.
 *   - src/core.ts §"validate" — output invariants every returned MPFR
 *     must satisfy.
 *   - src/ops/add.ts — reference for the same dispatch shape,
 *     validateArgs, roundMantissa, and packNormal helpers; mul.ts
 *     duplicates those helpers locally for now.
 *   - CLAUDE.md "Hallucination-risk callouts" — 0*Inf=NaN, ternary
 *     direction is sign of (rounded - exact) at the RESULT sign,
 *     rounding-mode count is FIVE, signed-zero is observable.
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
// Rounding primitive — duplicated from set_d.ts / get_d.ts / add.ts.
//
// As noted in add.ts, the third consumer triggers extraction to a
// shared `src/internal/mpfr/round_raw.ts`. mul.ts is the FOURTH
// consumer of this primitive (after set_d, get_d, add) — the
// extraction is now overdue. Per CLAUDE.md PIL.4 we do not refactor
// the substrate mid-Pilot; the extraction is filed as a follow-up
// against the Production-phase opening cleanup.
// ---------------------------------------------------------------------------

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
 * Ref: src/ops/add.ts § roundMantissa — identical body.
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
        // Tie: ties-to-even.
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

/**
 * Validate the public-boundary arguments. Throws `MPFRError` on bad
 * inputs. See add.ts § validateArgs for the rationale: we trust input
 * MPFR structure (the runner pre-validates via decodeMpfr; internal
 * callers produce pre-validated values) and only check the scalar
 * `prec` / `rnd` arguments here.
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
 *
 * Pre-conditions:
 *   - `sign` ∈ {1, -1}, `srcExp` is the unrounded magnitude's MPFR exp,
 *   - `srcMant` is an unsigned bigint MSB-aligned to `srcPrec` bits
 *     (i.e. `2^(srcPrec-1) <= srcMant < 2^srcPrec`),
 *   - `prec >= 1`.
 *
 * Ref: src/ops/add.ts § packNormal — identical body.
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
// Sign composition
// ---------------------------------------------------------------------------

/**
 * The product-of-signs rule. Mirrors MPFR's `MPFR_MULT_SIGN` macro
 * (mpfr/src/mpfr-impl.h — `MPFR_SIGN_POS * MPFR_SIGN_POS = MPFR_SIGN_POS`,
 * etc.; logically just integer multiplication of ±1 values).
 *
 * Returns `1` if signs match, `-1` if they differ. Used uniformly for
 * every non-NaN branch of mpfr_mul — zero, infinity, normal alike.
 */
function multSign(a: Sign, b: Sign): Sign {
  return (a * b) as Sign;
}

// ---------------------------------------------------------------------------
// Bit-width helper
// ---------------------------------------------------------------------------

/**
 * Number of significant bits in `v`. For `v > 0` this is the position
 * (1-indexed) of the topmost set bit. For `v === 0n` returns `0n`.
 *
 * Used to detect the "MSB-of-product is one bit short" case
 * (mpfr/src/mul.c L131 — `if (b1 == 0) mpn_lshift(tmp, tmp, tn, 1)`):
 * the product of two MSB-aligned p- and q-bit mantissas has bit-length
 * in {p+q-1, p+q}; the difference shifts the result exponent by -1.
 *
 * Uses bigint's `toString(2).length` which is the fastest portable
 * bit-length for bigint in V8/Bun — internally a single binary-radix
 * conversion, not an iterative shift. For 400-bit products (PREC_MAX-
 * neighbourhood) this is sub-microsecond; the alternative loop-based
 * helper in add.ts (introduced before this optimisation was
 * benchmarked) is kept there for consistency but mul.ts uses the
 * faster form.
 *
 * Ref: V8 BigInt-to-string is O(n^1.585) Karatsuba-bounded; for n=400
 * the constant beats a 400-iteration `while (x > 0n) { x >>= 1n; }`
 * loop comfortably.
 */
function bitLength(v: bigint): bigint {
  if (v <= 0n) return 0n;
  return BigInt(v.toString(2).length);
}

// ---------------------------------------------------------------------------
// Normal * normal core
// ---------------------------------------------------------------------------

/**
 * Multiply two normal MPFR values; return the {value, ternary} pair at
 * the target precision.
 *
 * Strategy: form the exact bigint product, determine the result
 * exponent from the product's bit-length (carry-out bit detection),
 * then round to `prec` via roundMantissa.
 *
 * Pre-conditions:
 *   - `a` and `b` are kind:'normal'.
 *   - `prec >= 1`, `rnd` is a valid RoundingMode.
 */
function mulNormalNormal(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // The product-of-signs rule applies uniformly. Even if the rounded
  // mantissa rounds to a value that "looks like" the magnitude of one
  // operand, the sign is the product — getting this wrong is the
  // most-common subtle ternary bug per CLAUDE.md "Hallucination-risk
  // callouts".
  const resultSign: Sign = multSign(a.sign, b.sign);

  // Exact bigint product. Both mantissas are non-negative integers
  // MSB-aligned to their own precisions; the product is a non-negative
  // integer with bit-length L ∈ {a.prec + b.prec - 1, a.prec + b.prec}.
  // This is the bigint equivalent of mpfr/src/mul.c L118–L120's
  // `mpn_mul(tmp, MPFR_MANT(b), bn, MPFR_MANT(c), cn)` call.
  const product = a.mant * b.mant;

  // Neither operand is normalised to zero (kind:'normal' implies mant
  // >= 2^(prec-1) > 0), so the product is strictly positive.
  if (product === 0n) {
    // Defensive: unreachable given the kind:'normal' invariant. Surface
    // as a precise error rather than silently produce a zero result
    // (which would mask a much deeper invariant violation).
    throw new MPFRError(
      'EPREC',
      'mulNormalNormal: zero product from normal operands (invariant violated)',
    );
  }

  const L = bitLength(product);
  // The exact value of |a*b| is product * 2^(a.exp - a.prec + b.exp -
  // b.prec); writing this in MPFR's normalised form |a*b| ∈
  // [2^(E-1), 2^E) we get E = (lowAnchor) + L where lowAnchor = a.exp
  // - a.prec + b.exp - b.prec, i.e.
  //
  //     resultExp = a.exp + b.exp + (L - a.prec - b.prec)
  //
  // The bracketed term is in {-1, 0} exactly: when both operands'
  // mantissas are near the top of their range (close to 2^prec) the
  // product gets the carry bit and L == a.prec + b.prec; otherwise
  // L == a.prec + b.prec - 1 and the exponent decrements by one. This
  // is mpfr/src/mul.c L131's `b1` carry-out bit handled in our bit-
  // width framing rather than a limb-MSB inspection.
  const resultExp = a.exp + b.exp + (L - a.prec - b.prec);

  // packNormal handles both the lossless (prec >= L) and rounding
  // (prec < L) paths; the round-up-by-1-ulp carry that pushes mant
  // past 2^prec is handled inside roundMantissa with the
  // `incremented === upperBound` branch.
  return packNormal(resultSign, resultExp, product, L, prec, rnd);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Multiply two MPFR values at the target precision, returning the
 * rounded result and the ternary flag (sign of `(rounded - exact)`).
 *
 * @mpfrName mpfr_mul
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
 *   mul(setD(2.0, 53n, 'RNDN').value, setD(3.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 6.0 at prec 53, ternary: 0}
 *   mul(posZero(53n), posInf(53n), 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}  — 0*Inf is indeterminate
 *   mul(negInf(53n), setD(-2.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: +Inf at prec 53, ternary: 0}  — sign-product
 */
export function mpfr_mul(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // --- Specials ---------------------------------------------------------
  // (1) NaN propagation. Cheapest discriminator first.
  if (a.kind === 'nan' || b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) ±Inf handling. The product-of-signs rule determines the
  // returned Inf's sign; the 0*Inf case is the canonical NaN.
  if (a.kind === 'inf') {
    if (b.kind === 'zero') {
      // 0 * Inf — indeterminate. Mirrors mpfr/src/mul.c L67–L72.
      return { value: NAN_VALUE, ternary: 0 };
    }
    // b is either inf or normal — both produce sign-product Inf.
    const sign = multSign(a.sign, b.sign);
    return {
      value: sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }
  if (b.kind === 'inf') {
    if (a.kind === 'zero') {
      // 0 * Inf — indeterminate (symmetric). Mirrors mpfr/src/mul.c
      // L82–L86.
      return { value: NAN_VALUE, ternary: 0 };
    }
    // a is normal (zero ruled out above, nan/inf earlier).
    const sign = multSign(a.sign, b.sign);
    return {
      value: sign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // (3) ±0 * ±0 — exact, sign = product of signs.
  // (4) ±0 * normal (or normal * ±0) — exact, sign = product of signs.
  // The C reference handles both via the same MPFR_SET_ZERO branch
  // (mpfr/src/mul.c L88–L94), and the sign-product rule is the same.
  // We dispatch with two checks (either operand zero) collapsing to
  // a single result-construction step.
  if (a.kind === 'zero' || b.kind === 'zero') {
    const sign = multSign(a.sign, b.sign);
    return {
      value: sign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (5) normal * normal — the algebraic core.
  if (a.kind !== 'normal' || b.kind !== 'normal') {
    // Defensive: unreachable. All other kinds dispatched above.
    throw new MPFRError(
      'EPREC',
      `mpfr_mul: unexpected kinds a=${a.kind} b=${b.kind} at normal-normal branch`,
    );
  }
  return mulNormalNormal(a, b, prec, rnd);
}
