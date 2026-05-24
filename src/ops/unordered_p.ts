/**
 * ops/unordered_p.ts — pure-TS port of MPFR's `mpfr_unordered_p`.
 *
 * "Unordered" predicate. Returns `true` iff at least one of the two
 * operands is NaN; `false` otherwise (every non-NaN/non-NaN pair is
 * orderable under MPFR's comparison).
 *
 * C signature
 * -----------
 *
 *   int mpfr_unordered_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   See mpfr/src/comparisons.c L75–L79:
 *
 *     int mpfr_unordered_p (mpfr_srcptr x, mpfr_srcptr y) {
 *       return MPFR_IS_NAN(x) || MPFR_IS_NAN(y);
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_unordered_p(a: MPFR, b: MPFR): boolean;
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Almost none — both sides are non-throwing, both return a boolean
 * (encoded as int in C). The TS surface preserves the non-throwing
 * NaN contract that the predicate family enjoys (see `src/ops/less_p.ts`
 * §"Divergence from C → TS"). The return type is TS `boolean` rather
 * than `int`; the grader's `jl_output_scalar_bool` wire helper emits
 * bare-JSON booleans to match.
 *
 * Algorithm
 * ---------
 *
 * One-line check: `a.kind === 'nan' || b.kind === 'nan'`. No call into
 * compareMPFR — the predicate doesn't need the full kind/sign/exp/mant
 * dispatch; it only inspects the discriminant.
 *
 * We DO NOT call `validate(a)` / `validate(b)` here because:
 *   - `unordered_p` is documented as "x and y are allowed to be out of
 *     range" (mpfr/src/comparisons.c L36) — the predicate exists
 *     specifically so callers can probe NaN-ness without first
 *     normalising operands.
 *   - The kind discriminant is the only field we read; a malformed value
 *     whose other fields violate the schema's MSB-alignment etc. would
 *     still have a well-defined `kind`, and unordered_p's answer would
 *     still be correct.
 *
 * (Contrast with the strict cmp family — `mpfr_cmp` and friends call
 * `validate` because they read every field. The predicate family's
 * shared `compareMPFR` core also validates, but unordered_p doesn't go
 * through that core.)
 *
 * Refs
 * ----
 *
 *   - mpfr/src/comparisons.c L75–L79 — the C reference.
 *   - mpfr/src/comparisons.c L24–L37 — the truth table comment showing
 *     unordered_p's row: "0  0  0  1" (false for less/equal/greater,
 *     true only on NaN).
 *   - src/ops/less_p.ts — sibling predicate; the non-throwing-NaN
 *     contract is shared.
 *   - mpfr/tests/tcomparisons.c L26–L84 — source for the `mined` cases.
 *     The cmp_tests routine asserts `unordered_p == (cmpbool == 0x40)`
 *     for the NaN-only branch.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — unordered_p
 *     returning `true` on NaN/anything IS the IEEE 754 "NaN is unordered"
 *     rule; the boolean direction is correct here.
 */

import type { MPFR } from '../core.ts';

/**
 * Test whether `a` and `b` are unordered (i.e. at least one is NaN).
 *
 * @param a left operand.
 * @param b right operand.
 * @returns `true` iff `a.kind === 'nan' || b.kind === 'nan'`; `false`
 *   otherwise. Never throws (no input is ever invalid for this
 *   predicate — see the module docstring).
 *
 * @mpfrName mpfr_unordered_p
 */
export function mpfr_unordered_p(a: MPFR, b: MPFR): boolean {
  // One-line discriminant check. No call into compareMPFR — the
  // predicate doesn't need ordering, just NaN detection. The
  // short-circuit evaluation of `||` saves the second discriminant
  // read on the common case where `a` is NaN.
  return a.kind === 'nan' || b.kind === 'nan';
}
