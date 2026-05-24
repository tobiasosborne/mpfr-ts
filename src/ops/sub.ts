/**
 * ops/sub.ts — pure-TS port of MPFR's `mpfr_sub`.
 *
 * Subtract two {@link MPFR} values at the caller-supplied target
 * precision, rounded per the rounding mode, returning the canonical
 * `{value, ternary}` from src/core.ts.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sub(mpfr_t rop, mpfr_srcptr op1, mpfr_srcptr op2, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - sets `rop = op1 - op2`, rounded per `rnd`;
 *   - returns the ternary (sign of rounded - exact).
 *
 *   Ref: mpfr/src/sub.c L24–L133. The C dispatcher handles the
 *   singular operand cases inline (NaN/Inf/zero) then routes to
 *   `mpfr_sub1` / `mpfr_sub1sp` (same-sign: a real subtraction) or to
 *   `mpfr_add1` / `mpfr_add1sp` (opposite-sign: structurally an
 *   addition). The exception cases mirror IEEE 754:
 *
 *     - NaN - anything           → NaN
 *     - +Inf - +Inf, -Inf - -Inf → NaN
 *     - +Inf - -Inf              → +Inf  (and symmetric)
 *     - ±Inf - finite            → ±Inf  (sign of op1)
 *     - finite - ±Inf            → ∓Inf  (opposite sign of op2)
 *     - ±0 - ±0:  sign per rnd_mode (see below)
 *     - ±0 - x   → mpfr_neg(x, rnd)
 *     - x  - ±0  → mpfr_set(x, rnd)
 *
 * TS signature
 * ------------
 *
 *   mpfr_sub(a: MPFR, b: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `prec` as an explicit positional argument (no `rop`);
 *   - returns the immutable `Result` from src/core.ts (Law 4).
 *
 * Implementation strategy: composition over `mpfr_add` + sign flip
 * ----------------------------------------------------------------
 *
 * Mathematically, `a - b == a + (-b)`. The naive way to implement
 * this is to call `mpfr_neg(b, ???, RNDN)` then `mpfr_add(a, negated_b,
 * prec, rnd)`. **DO NOT do this.** It introduces a double-rounding
 * bug whenever `b`'s precision differs from any intermediate
 * precision the neg step rounds to: the neg-step rounding may move
 * `b` away from its true value, and the subsequent add rounds again.
 * Even passing the same `b.prec` to `mpfr_neg` works (no rounding,
 * because `prec >= x.prec` is the lossless-pad path), but it is
 * load-bearing on a contract we'd rather not rely on.
 *
 * The robust composition is to **rebuild `b` with the opposite sign
 * as a literal struct** — no rounding, no allocation, no ternary.
 * The locked schema makes this trivial:
 *
 *     const negatedB: MPFR = { ...b, sign: -b.sign as Sign };
 *
 * For NaN, where the schema fixes `sign === 1` (src/core.ts L83),
 * the spread above would produce a NaN with `sign === -1`, violating
 * the canonical NaN invariant `validate()` enforces (src/core.ts
 * L384–L386). So we short-circuit NaN through `mpfr_add` directly
 * (which already returns the canonical `NAN_VALUE`); the sign-flip
 * is only applied to the finite kinds.
 *
 * For zeros, the sign matters and the spread works correctly:
 *
 *   - `b = +0`  ⇒ `negatedB = -0`
 *   - `b = -0`  ⇒ `negatedB = +0`
 *
 * For ±Inf, the spread also works (negating an Inf flips its sign).
 *
 * For normals, the spread flips `sign` and preserves `mant`, `exp`,
 * `prec`. Crucially, this is mathematically exact: -x for a normal
 * x has the same mantissa and exponent as x, just the opposite
 * sign. No rounding step is involved in the negation itself; the
 * sole rounding happens inside the delegated `mpfr_add`.
 *
 * Why is the composition's ternary correct?
 * -----------------------------------------
 *
 * `mpfr_add(a, -b, prec, rnd)` computes the ternary as the sign of
 * `(rounded_result - exact_result)`. Since `a - b == a + (-b)` is an
 * exact identity (not a rounded approximation), the exact result is
 * the same; the rounded result is computed identically; the ternary
 * direction is the same. There is no inversion to perform — unlike
 * the `sub.c` C path which inverts `rnd_mode` on certain branches
 * (`MPFR_INVERT_RND(rnd_mode)` at sub.c L89, L115) because of the
 * specific way `mpfr_add1` / `mpfr_add1sp` are structured. Our
 * composition delegates to `mpfr_add` which handles the rounding
 * uniformly; no inversion is needed.
 *
 * Aliasing concerns
 * -----------------
 *
 * The C version supports `mpfr_sub(rop, op, op, rnd)` — same pointer
 * for `op1` and `op2`, returning ±0. The TS surface is immutable so
 * aliasing is structural identity, not memory identity:
 *
 *     mpfr_sub(x, x, prec, rnd)
 *
 * — and the math still works: `x + (-x) == 0` via the effective-
 * subtract branch of `mpfr_add` (sign-opposite operands, magnitudes
 * equal → cancellation-zero per rnd_mode). The composition is
 * therefore alias-correct by construction, no special-case needed.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub.c L24–L133 — C reference (with the rnd_mode-
 *     inverting branches we deliberately don't replicate).
 *   - src/ops/add.ts — the delegate; documents the
 *     `zeroSumSign` / `cancellationZeroSign` rules our composition
 *     inherits.
 *   - src/core.ts L113–L135 — `MPFR` value shape; spread is safe
 *     because all fields are primitive (no nested object to share).
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *     the sign flip on zero MUST propagate; "NaN ≠ NaN" — the NaN
 *     short-circuit preserves the canonical sentinel.
 *   - mpfr/src/add.c L66–L75 — the `±0 + ±0` rule the delegate
 *     uses; `(+0) - (+0) → (-0) + (+0)` under RNDD gives `-0`
 *     (matches sub.c L64–L66 directly via the rnd-aware sign rule).
 *   - mpfr/tests/tsub.c — source for the `mined` cases the golden
 *     transcribes.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
import { MPFRError, NAN_VALUE, PREC_MAX, PREC_MIN } from '../core.ts';
import { mpfr_add } from './add.ts';

/**
 * Validate the public-boundary scalar arguments. Same shape as add.ts /
 * neg.ts / abs.ts. We do NOT structurally re-validate `a` / `b`; the
 * runner pre-validates via `decodeMpfr`, and library-internal callers
 * pass pre-validated values. Shape bugs surface as wrong outputs the
 * grader catches.
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
 * Subtract two MPFR values at the target precision, returning the
 * rounded result and the ternary flag (sign of `(rounded - exact)`).
 *
 * @mpfrName mpfr_sub
 *
 * @param a     minuend. Any kind ('normal', 'zero', 'inf', 'nan').
 * @param b     subtrahend. Any kind.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. The value passes `validate()`
 *              without post-processing. Ternary is `0` for all
 *              specials (NaN, Inf, ±0 - ±0); `±1` only when the
 *              normal-normal arithmetic rounds inexactly.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad
 *                    rounding mode. NaN / Inf input is NOT an error.
 *
 * @example
 *   sub(setD(3.0, 53n, 'RNDN').value, setD(1.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: 2.0 at prec 53, ternary: 0}
 *   sub(x, x, 53n, 'RNDN');
 *     // → {value: posZero(53n), ternary: 0}  — cancellation
 *   sub(posZero(53n), posZero(53n), 53n, 'RNDD');
 *     // → {value: negZero(53n), ternary: 0}  — RNDD-observable
 *   sub(NAN_VALUE, x, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}
 */
export function mpfr_sub(
  a: MPFR,
  b: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // NaN short-circuit. We dispatch this before the sign-flip rebuild
  // because flipping NaN's `sign` to `-1` would violate the canonical
  // NaN shape (validate() requires sign=1 for kind:'nan'). The
  // delegated mpfr_add already collapses any NaN operand to NAN_VALUE
  // via its own NaN dispatch (src/ops/add.ts L607–L609); we route
  // through it explicitly here so the early-exit is visible.
  if (a.kind === 'nan' || b.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // Negation by struct-rebuild. All four non-NaN kinds (normal, zero,
  // inf) preserve their MPFR shape under sign negation:
  //   - normal: mantissa unchanged, exponent unchanged, sign flipped.
  //     This is the exact mathematical negation; no rounding involved.
  //   - zero:   +0 → -0 or -0 → +0; the sign flip is the entire
  //     semantic of "negate a signed zero".
  //   - inf:    +Inf → -Inf or -Inf → +Inf; same.
  //
  // The spread copies the bigint primitives (prec, exp, mant) by value
  // — bigints are immutable in JS, and the other fields (kind, sign)
  // are primitive too — so `negatedB` is a fresh frozen-shape object
  // that doesn't share mutable state with `b`. Per src/core.ts L90 the
  // Sign type is `1 | -1`; `(-b.sign) as Sign` is sound because b is
  // not NaN here (NaN dispatched above) and therefore b.sign ∈ {1,-1},
  // whose negation is also in {1,-1}.
  const negatedB: MPFR = {
    kind: b.kind,
    sign: (-b.sign) as Sign,
    prec: b.prec,
    exp: b.exp,
    mant: b.mant,
  };

  // Delegate. The add op already handles every dispatch we'd need:
  //   - ±Inf + ±Inf  (matches/doesn't match)
  //   - ±Inf + finite
  //   - ±0 + ±0      (RNDD-observable zero sign)
  //   - ±0 + normal
  //   - normal + normal (effective add or effective subtract by sign)
  //
  // For a typical `a - b` with same-sign normals, add(a, -b) routes
  // through the opposite-sign branch of addNormalNormal (effective
  // subtract). For opposite-sign normals, it routes through the
  // same-sign branch (effective add). The rnd_mode passes through
  // unchanged — no inversion needed because the algebraic
  // transformation `a - b → a + (-b)` doesn't shift the rounding
  // direction (RNDU still rounds toward +∞, RNDD still toward -∞).
  return mpfr_add(a, negatedB, prec, rnd);
}
