/**
 * internal/mpfr/round_raw.ts — substrate primitive for MPFR mantissa rounding.
 *
 * Pure-TS analog of MPFR's `mpfr_round_raw_generic` family. Takes an
 * MSB-aligned `srcPrec`-bit source mantissa, drops `srcPrec - outPrec`
 * low bits per the requested rounding mode and value sign, and returns
 * the rounded mantissa together with its (possibly carried-out) exponent
 * and the ternary flag.
 *
 * This module is the **substrate** for every public op that rounds a
 * bigint mantissa down to a target precision. Before this extraction the
 * same body was duplicated four times — in `set_d.ts`, `get_d.ts`,
 * `add.ts`, and `mul.ts`. The fourth copy in `mul.ts` triggered the
 * extraction per the convention noted in each of those files.
 *
 * Substrate contract
 * ------------------
 *
 * Per CLAUDE.md Law 3 ("faithful substrate, idiomatic surface"), this
 * module:
 *
 *   - Imports from `src/core.ts` (`RoundingMode`, `Sign`, `Ternary`,
 *     `MPFRError`) because those types and the rounding-mode enum
 *     belong to the locked schema — the substrate speaks the same
 *     `RoundingMode` values the public ops do.
 *   - Has no I/O, no module-level state, no Bun/Node imports. Pure
 *     bigint arithmetic. Safe to import from anywhere in the library.
 *   - Mirrors mpfr/src/round_raw_generic.c's bit-test logic exactly,
 *     in the single-bigint framing rather than the C limb-array framing.
 *
 * Algorithm
 * ---------
 *
 * Let `k = srcPrec - outPrec` (the number of low bits to drop).
 * Let `trunc = srcMant >> k` (the truncated mantissa, in
 *   `[2^(outPrec-1), 2^outPrec)`).
 * Let `dropped = srcMant & (2^k - 1)` (the bits we're dropping).
 *
 * `exact` iff `dropped == 0`. Otherwise the round step is:
 *
 *   - RNDZ:  keep `trunc`. Ternary: sign=+1 → -1 (rounded < exact);
 *            sign=-1 → +1 (rounded > exact; less negative).
 *
 *   - RNDA:  `trunc + 1`. Ternary: sign=+1 → +1; sign=-1 → -1.
 *
 *   - RNDD:  truncate iff sign=+1; increment iff sign=-1. Same ternary
 *            as RNDZ/RNDA in the relevant branches.
 *
 *   - RNDU:  symmetric to RNDD.
 *
 *   - RNDN:  tie = `dropped == 2^(k-1)` exactly. Strictly above the
 *            half → increment. Strictly below → truncate. Tie → break to
 *            even (increment iff `trunc`'s LSB is 1).
 *
 * Carry-out: when `trunc + 1 == 2^outPrec` the increment overflows the
 * MSB-alignment frame. Renormalise by shifting right one position (the
 * result becomes `2^(outPrec-1)`) and bumping the exponent by 1. The
 * numeric value is unchanged; only the storage form is normalised.
 *
 * Ternary direction (CLAUDE.md hallucination callout): the returned
 * `ternary` is the sign of `(rounded - exact)`, NOT
 * `(exact - rounded)`. For a truncated positive (rounded < exact) we
 * return `-1`; for a truncated negative (rounded > exact, i.e. less
 * negative) we return `+1`. Inverting this is the single most subtle
 * porting bug.
 *
 * Pre-conditions (enforced by callers; checked defensively where cheap)
 * ---------------------------------------------------------------------
 *
 *   - `srcMant >= 2^(srcPrec - 1)` and `srcMant < 2^srcPrec` —
 *     MSB-aligned to `srcPrec` bits.
 *   - `srcPrec > outPrec` — the lossless `srcPrec <= outPrec` case is
 *     handled by the caller without entering this function (a left-shift
 *     padding step, which is exact and has ternary 0).
 *   - `outPrec >= 1`.
 *   - `sign` is `1` or `-1`; it determines the RNDU/RNDD branch and the
 *     sign of the returned ternary.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/round_raw_generic.c — the canonical C primitive. The
 *     limb-array `rnd_away` / `rnd_inex` machinery there reduces to the
 *     `increment`/`!increment` decision below in single-bigint form.
 *   - src/core.ts — locked `RoundingMode`, `Sign`, `Ternary` types.
 *   - CLAUDE.md "Hallucination-risk callouts" — ternary direction is
 *     sign of (rounded - exact); rounding-mode count is FIVE
 *     (no RNDF, no RNDNA).
 *   - Previous duplicated definitions in src/ops/{set_d, get_d, add,
 *     mul}.ts — replaced by an import of this module's
 *     `roundMantissa`.
 */

import type { RoundingMode, Sign, Ternary } from '../../core.ts';
import { MPFRError } from '../../core.ts';

/**
 * Result of {@link roundMantissa}. The shape is what every consumer
 * packs into a `Result.value` (a normal MPFR with this mant/exp at the
 * out-prec) and a `Result.ternary`.
 *
 * - `mant`: the rounded mantissa, MSB-aligned to `outPrec` bits
 *   (`2^(outPrec-1) <= mant < 2^outPrec`).
 * - `exp`: the post-round MPFR exponent. Equals `srcExp` for the
 *   non-carrying branch; equals `srcExp + 1` when an LSB increment
 *   overflowed past `2^outPrec`.
 * - `ternary`: sign of `(rounded - exact)` — `-1`, `0`, or `+1`.
 */
export interface RoundedMantissa {
  readonly mant: bigint;
  readonly exp: bigint;
  readonly ternary: Ternary;
}

/**
 * Round `srcMant` (an MSB-aligned `srcPrec`-bit non-negative bigint)
 * down to `outPrec` bits using `rnd` and the value's `sign`. Returns the
 * rounded mantissa, the (possibly carried) exponent, and the ternary
 * flag.
 *
 * Pre-conditions: see the module-level docstring. Caller is responsible;
 * we do not re-validate every invocation (this is a hot path).
 *
 * Behaviour for an exhaustive enum-narrowing miss (an unknown rounding
 * mode at runtime — possible only if the caller skipped its own
 * `validateArgs` check) is to throw `MPFRError('EROUND', ...)` rather
 * than fall through silently. This matches the convention in the public
 * ops.
 *
 * @param srcMant  MSB-aligned source mantissa as an unsigned bigint.
 * @param srcPrec  Source precision (number of bits in `srcMant`).
 * @param srcExp   The pre-round MPFR exponent.
 * @param outPrec  Target precision. Must be `< srcPrec` and `>= 1`.
 * @param sign     Sign of the unrounded value (`1` or `-1`).
 * @param rnd      One of the five {@link RoundingMode} values.
 *
 * @returns        `{mant, exp, ternary}` per {@link RoundedMantissa}.
 *
 * @throws {MPFRError} `EROUND` on an unrecognised rounding mode (defensive).
 *
 * Ref: mpfr/src/round_raw_generic.c — canonical bit-test logic for
 *   all five modes, in limb-array form.
 */
export function roundMantissa(
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
    // Exact: the low k bits are all zero, so truncation IS the exact
    // value. Ternary 0; no carry possible.
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
      // Toward -∞: increment iff negative (so magnitude grows, signed
      // value drops below the exact). Positive truncates.
      increment = sign === -1;
      break;
    case 'RNDU':
      // Toward +∞: increment iff positive. Negative truncates (magnitude
      // shrinks, signed value rises above the exact).
      increment = sign === 1;
      break;
    case 'RNDN': {
      // Half-ulp boundary is 2^(k-1). dropped > half → round up;
      // dropped < half → round down; dropped == half (the tie) → break
      // to even (increment iff trunc's LSB is 1).
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
      // Exhaustiveness guard. The TS `RoundingMode` union narrows to
      // `never` here; the runtime throw covers a caller that bypassed
      // its own `validateArgs`.
      const _exhaustive: never = rnd;
      void _exhaustive;
      throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
    }
  }

  if (!increment) {
    // Truncating: rounded magnitude < exact magnitude.
    // sign=+1: rounded < exact → ternary -1.
    // sign=-1: rounded > exact (less negative) → ternary +1.
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
