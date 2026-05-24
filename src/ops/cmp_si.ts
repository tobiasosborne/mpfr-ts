/**
 * ops/cmp_si.ts — pure-TS port of MPFR's `mpfr_cmp_si`.
 *
 * Compare an {@link MPFR} value against a signed machine integer.
 * Returns `-1` if `x < n`, `0` if `x == n`, `+1` if `x > n`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp_si(mpfr_srcptr b, long int i);
 *
 *   The C entry point at mpfr/src/cmp_si.c L120–L123 is a trivial wrapper
 *   over `mpfr_cmp_si_2exp(b, i, 0)`. The body (L33–L98) dispatches:
 *
 *     1. NaN x: set erange flag, return 0.
 *     2. ±Inf x: return MPFR_INT_SIGN(x).
 *     3. ±0 x: return `i != 0 ? -sign(i) : 0`.
 *     4. sign(x) != sign(i) || i == 0: return MPFR_INT_SIGN(x).
 *     5. Otherwise: compare magnitudes via exponent then limb compare.
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmp_si(x: MPFR, n: bigint): number;
 *
 *   - `n` is a `bigint` in `[LONG_MIN, LONG_MAX]` = `[-(2^63), 2^63 - 1]`.
 *     Matches `mpfr_set_si`'s argument convention (JS `number` can't hold
 *     the full int64 range losslessly).
 *   - Returns a plain JS `number` in `{-1, 0, +1}` — same shape as
 *     {@link import('./cmp.ts').mpfr_cmp}.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Same divergence as `mpfr_cmp` (see src/ops/cmp.ts §"Divergence from C
 * → TS"): NaN `x` THROWS `MPFRError('EDOMAIN', ...)` rather than
 * silently returning 0 with the erange side-channel flag. Callers who
 * need the C semantics wrap in a try/catch.
 *
 * Out-of-range `n` (outside `[LONG_MIN, LONG_MAX]`) throws
 * `MPFRError('EPREC', ...)` — matches `mpfr_set_si`'s range-check
 * pattern (same EPREC-as-"bad argument" rationale).
 *
 * Algorithm
 * ---------
 *
 * Rather than re-implementing the C reference's exponent/limb dispatch
 * for the (x normal, n nonzero, signs match) branch, we build a temp
 * MPFR exact-representation of `n` at prec = `bitLength(|n|)` and
 * delegate to {@link compareMPFR}. The two routes produce identical
 * results because:
 *
 *   - When `n === 0n`: tempMPFR is `+0`. compareMPFR's "exactly one zero
 *     (the other normal)" branch returns `-b.sign` if `b` is normal,
 *     `+1`/-1/0 per `x.sign` if `x` is normal/inf/zero — matching the C
 *     reference's `i == 0 → MPFR_INT_SIGN(x)` and `MPFR_IS_ZERO(b) → 0`
 *     branches exactly.
 *
 *   - When `n` is nonzero: tempMPFR is a normal MPFR. compareMPFR's
 *     normal/normal branch (sign → exp → MSB-aligned mantissa compare)
 *     produces the same answer as cmp_si's limb-shift comparison, since
 *     the underlying value identity is `n = sign(n) * |n| * 2^0` with
 *     mantissa width `bitLength(|n|)`, exp = `bitLength(|n|)` (so the
 *     schema's `[2^(exp-1), 2^exp)` envelope contains `|n|`).
 *
 * Why is this safe?
 * -----------------
 *
 * The temp MPFR is exact (no rounding occurs when storing `n` at prec
 * = `bitLength(|n|)`). The MPFR-vs-MPFR comparison in compareMPFR is
 * the canonical algorithm — it handles every kind/sign/exp/mantissa
 * combination uniformly. The performance cost of constructing the
 * temp is one bigint-allocation per call, negligible for the
 * arithmetic-class 50 ms budget.
 *
 * NaN propagation: compareMPFR returns `null` for NaN. We catch that
 * here at the boundary and translate to an EDOMAIN throw — the same
 * way `mpfr_cmp` does. Because `n` is a `bigint` and cannot itself be
 * NaN, the only NaN path is `x.kind === 'nan'`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmp_si.c L33–L98 — the C reference (mpfr_cmp_si_2exp
 *     with f=0). The "MPFR_LONG_WITHIN_LIMB" branch (L57–L98) is the
 *     limb-shift compare; the alternative branch (L99–L114) builds a
 *     temp MPFR via `mpfr_set_si_2exp` and delegates to mpfr_cmp,
 *     which is exactly the strategy we use unconditionally.
 *   - mpfr/src/cmp_si.c L120–L123 — the public entry.
 *   - src/internal/mpfr/cmp_raw.ts — the shared comparison core.
 *   - src/ops/cmp.ts — the throwing surface for the MPFR-vs-MPFR case.
 *   - src/ops/set_si.ts — the n-to-MPFR exact conversion this port
 *     mirrors structurally (the zero shortcut + bit-length + MSB-aligned
 *     mantissa construction).
 *   - CLAUDE.md "Hallucination-risk callouts": NaN ≠ NaN (throw, don't
 *     return 0); ternary direction does not apply here (no rounding);
 *     rounding-mode count is FIVE (also N/A; no `rnd` parameter).
 */

import type { MPFR, Sign } from '../core.ts';
import { MPFRError } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Smallest signed 64-bit integer: `-(2^63)`. Matches the C `LONG_MIN`
 * on the platforms we target (CLAUDE.md Rule 12).
 */
const LONG_MIN_VAL: bigint = -(1n << 63n);

/**
 * Largest signed 64-bit integer: `2^63 - 1`. Matches `LONG_MAX`.
 */
const LONG_MAX_VAL: bigint = (1n << 63n) - 1n;

/**
 * Bit length of a non-negative bigint (position of topmost set bit,
 * 1-indexed). Returns 0 for `0n`. Bounded by 64 iterations given the
 * int64 range.
 */
function bitLength(n: bigint): bigint {
  let bits = 0n;
  let probe = n;
  while (probe > 0n) {
    bits++;
    probe >>= 1n;
  }
  return bits;
}

/**
 * Compare an {@link MPFR} value against a signed integer.
 *
 * @param x  MPFR value. Must pass {@link import('../core.ts').validate}.
 * @param n  bigint in `[LONG_MIN, LONG_MAX]`.
 * @returns `-1` if `x < n`, `0` if `x == n`, `+1` if `x > n`.
 *
 * @throws {MPFRError} `EDOMAIN` if `x.kind === 'nan'`. This diverges
 *   from the C reference (which sets erange + returns 0); the
 *   rationale is in src/ops/cmp.ts §"Divergence from C → TS".
 * @throws {MPFRError} `EPREC` if `n` is not a bigint or lies outside
 *   `[LONG_MIN, LONG_MAX]`.
 *
 * @mpfrName mpfr_cmp_si
 */
export function mpfr_cmp_si(x: MPFR, n: bigint): number {
  // --- Boundary validation ---------------------------------------------------
  // We do not pre-validate `x` here; compareMPFR's structural check on
  // both operands runs `validate(x)` and surfaces a precise EPREC if x
  // is malformed. Front-loading the `n` range check matches the pattern
  // in set_si — the error points at this function, not at a helper it
  // happens to call.
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < LONG_MIN_VAL || n > LONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of int64 range [${LONG_MIN_VAL}, ${LONG_MAX_VAL}], got ${n}`,
    );
  }

  // --- NaN x → throw, mirroring mpfr_cmp -------------------------------------
  // We check x.kind directly rather than letting compareMPFR translate
  // null to throw — the error message can mention `n` here, and we
  // avoid building an unnecessary temp MPFR for the throw path.
  if (x.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_si: NaN x (cmp_si requires a non-NaN MPFR operand)`,
    );
  }

  // --- Build a temp MPFR exact-representation of n ---------------------------
  // The temp MPFR represents `n` exactly (no rounding) because prec =
  // bitLength(|n|) holds the full integer magnitude. The schema's
  // value formula `sign * mant * 2^(exp - prec)` with mant = |n|, prec
  // = bitLength(|n|), exp = bitLength(|n|) yields `sign * |n| * 2^0`
  // = n. The MSB-alignment invariant `mant >= 2^(prec-1)` holds because
  // the top bit of |n| sits at position bitLength(|n|) - 1 = prec - 1.
  //
  // For n === 0n, we build a +0 MPFR with prec=1 (the minimum valid
  // precision). compareMPFR's zero-vs-anything dispatch (steps 2 and 5
  // in src/internal/mpfr/cmp_raw.ts) handles this uniformly: x.kind
  // === 'zero' && temp === 'zero' → 0; x.kind === 'normal' && temp
  // === 'zero' → x.sign; x.kind === 'inf' && temp === 'zero' → x.sign.
  // All three match the C reference's i==0 branch (L53–L54: return
  // MPFR_INT_SIGN(x), which is +1 for +x and -1 for -x, including
  // for zero — but the C side's MPFR_IS_ZERO short-circuit at L47–L48
  // gives 0 for +0/-0 paired with i=0, also matching).
  let temp: MPFR;
  if (n === 0n) {
    // posZero(1n) shape inlined. We don't call posZero() to avoid the
    // import/circularity and because the prec choice is arbitrary
    // (any valid precision yields the same compareMPFR result for a
    // zero operand).
    temp = { kind: 'zero', sign: 1, prec: 1n, exp: 0n, mant: 0n };
  } else {
    const sign: Sign = n < 0n ? -1 : 1;
    const absN: bigint = n < 0n ? -n : n;
    const bits = bitLength(absN);
    temp = {
      kind: 'normal',
      sign,
      prec: bits,
      exp: bits,
      mant: absN,
    };
  }

  // --- Delegate to compareMPFR -----------------------------------------------
  // compareMPFR validates both operands structurally and returns null
  // on NaN. We've already ruled out NaN x; n is bigint and cannot be
  // NaN. The non-null branch returns -1 / 0 / +1.
  const r = compareMPFR(x, temp);
  if (r === null) {
    // Unreachable: x is non-NaN (checked above) and temp is non-NaN
    // by construction. The defensive throw protects against a future
    // schema change that lets compareMPFR return null for some other
    // edge — surfaces cleanly rather than silently returning the
    // numeric zero of `null !== null` arithmetic.
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_si: compareMPFR returned null unexpectedly (x.kind=${x.kind})`,
    );
  }
  return r;
}
