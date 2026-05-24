/**
 * ops/cmpabs_ui.ts — pure-TS port of MPFR's `mpfr_cmpabs_ui`.
 *
 * Compare the absolute value of an {@link MPFR} against an unsigned integer.
 * Returns a positive number if `|b| > c`, `0` if `|b| == c`, and a negative
 * number if `|b| < c`. Return value is normalised to `{-1, 0, +1}`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmpabs_ui(mpfr_srcptr b, unsigned long c);
 *
 *   Ref: mpfr/src/cmpabs_ui.c L28–L34 — the C reference body.
 *   The implementation aliases `b` through `MPFR_TMP_INIT_ABS` (which
 *   strips the sign without copying the mantissa) and delegates directly
 *   to `mpfr_cmp_ui(absb, c)`.
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmpabs_ui(b: MPFR, c: bigint): number;
 *
 *   - `c` is a non-negative `bigint` in `[0, 2^64 - 1]` (matches `unsigned long`).
 *   - Sign of `b` is ignored — only `|b|` is compared.
 *   - Returns a plain JS `number` in `{-1, 0, +1}`.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Same divergence as `mpfr_cmp_ui`: NaN `b` THROWS `MPFRError('EDOMAIN', ...)`
 * rather than silently returning 0 with the erange flag set. Out-of-range `c`
 * (negative or above ULONG_MAX) throws `MPFRError('EPREC', ...)`.
 *
 * Algorithm
 * ---------
 *
 * The C implementation is a one-liner: build a temp `absb` with the same
 * precision, exponent, and mantissa as `b` but with sign forced to `+1`,
 * then call `mpfr_cmp_ui(absb, c)`.
 *
 * In TypeScript we replicate this directly by calling the existing
 * {@link mpfr_cmp_ui} with a synthesised positive copy of `b`:
 *
 *   1. If `b.kind === 'nan'`: throw `EDOMAIN` (same policy as `mpfr_cmp_ui`
 *      propagated through the alias).
 *   2. Build `absb` = `{ ...b, sign: 1 }` — identical to `MPFR_TMP_INIT_ABS`.
 *   3. Delegate to `mpfr_cmp_ui(absb, c)`.
 *   4. Normalise the returned int to `{-1, 0, +1}`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmpabs_ui.c L28–L34 — C reference body.
 *   - mpfr/src/cmp_ui.c L33–L92 — `mpfr_cmp_ui` dispatch (the delegate).
 *   - src/ops/cmp_ui.ts — the TS delegate port.
 *   - src/ops/cmpabs.ts — sibling MPFR↔MPFR absolute comparison.
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN" — NaN throws.
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *       cmpabs ignores sign by definition; `|+0| == |-0|`.
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';
import { mpfr_cmp_ui } from './cmp_ui.ts';

/**
 * Compare the absolute value of `b` against the unsigned integer `c`.
 *
 * @param b  MPFR value. Must pass {@link import('../core.ts').validate}.
 * @param c  Non-negative bigint in `[0, 2^64 - 1]` (unsigned long range).
 * @returns  A number in `{-1, 0, +1}`: positive if `|b| > c`, `0` if `|b| == c`,
 *           negative if `|b| < c`.
 *
 * @throws {MPFRError} `EDOMAIN` if `b.kind === 'nan'`.
 * @throws {MPFRError} `EPREC` if `c` is not a bigint or lies outside
 *   `[0, 2^64 - 1]`.
 *
 * @mpfrName mpfr_cmpabs_ui
 */
export function mpfr_cmpabs_ui(b: MPFR, c: bigint): number {
  // Ref: mpfr/src/cmpabs_ui.c L28–L34 — MPFR_TMP_INIT_ABS aliases b
  //   with sign forced to positive, then delegates to mpfr_cmp_ui.

  // --- NaN check (before alias construction) ---------------------------------
  // mpfr_cmp_ui (the delegate) will also throw on NaN, but we check here for
  // clarity and to match the C behaviour where the alias is built before any
  // dispatch. The erange-flag / throw policy is inherited from the delegate.
  if (b.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      'mpfr_cmpabs_ui: NaN operand b (cmpabs_ui requires a non-NaN MPFR operand)',
    );
  }

  // --- Build absb = |b| by forcing sign to +1 --------------------------------
  // Ref: mpfr/src/cmpabs_ui.c L31 — MPFR_TMP_INIT_ABS(absb, b)
  //   copies b's struct fields but overrides the sign field to positive.
  //   We replicate this with a spread that overrides only sign.
  const absb: MPFR = { ...b, sign: 1 };

  // --- Delegate to mpfr_cmp_ui and normalise ---------------------------------
  // Ref: mpfr/src/cmpabs_ui.c L33 — return mpfr_cmp_ui(absb, c)
  // mpfr_cmp_ui already returns values in {-1, 0, +1}; normalise for safety.
  const raw = mpfr_cmp_ui(absb, c);
  if (raw < 0) return -1;
  if (raw > 0) return 1;
  return 0;
}
