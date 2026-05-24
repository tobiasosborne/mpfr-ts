/**
 * ops/cmp_d.ts — pure-TS port of MPFR's `mpfr_cmp_d`.
 *
 * Compare an {@link MPFR} value against an IEEE 754 binary64 (`number`).
 * Returns `-1` if `x < d`, `0` if `x == d`, `+1` if `x > d`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp_d(mpfr_srcptr b, double d);
 *
 *   The C body (mpfr/src/cmp_d.c L24–L44) builds a temp MPFR at
 *   `IEEE_DBL_MANT_DIG` (53 bits) via `mpfr_set_d(tmp, d, MPFR_RNDN)` —
 *   which is EXACT because 53 bits exactly captures every binary64 —
 *   and delegates to `mpfr_cmp(b, tmp)`. The MPFR_ASSERTD ensures the
 *   set_d ternary is zero (exactness check).
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmp_d(x: MPFR, d: number): number;
 *
 *   - `d` is a JS `number` (binary64).
 *   - Returns a plain JS `number` in `{-1, 0, +1}`.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * Two NaN paths, both THROW `MPFRError('EDOMAIN', ...)`:
 *
 *   - `x.kind === 'nan'`: same as `mpfr_cmp` / `mpfr_cmp_si` / `mpfr_cmp_ui`
 *     (C: erange + return 0; we throw).
 *
 *   - `Number.isNaN(d)`: the C reference's `mpfr_set_d(tmp, NaN, RNDN)`
 *     produces an MPFR-NaN tmp, which then triggers the NaN-vs-anything
 *     erange path in mpfr_cmp. Our `mpfr_set_d` correctly produces a
 *     NAN_VALUE for NaN input — but rather than building the temp and
 *     deferring to compareMPFR (which would return `null`), we
 *     short-circuit and throw directly here. Same observable behaviour
 *     (a throw), one fewer allocation, clearer error message
 *     ("NaN d" vs "compareMPFR returned null").
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `d` is a `number` at the type boundary; range-check
 *      against NaN (throws).
 *   2. NaN `x` → throw EDOMAIN.
 *   3. Build a temp MPFR via {@link mpfr_set_d}(d, 53n, 'RNDN'). The
 *      conversion is EXACT for any non-NaN binary64 because:
 *
 *        - ±0 → ±0 MPFR (signed zero preserved, ternary 0).
 *        - ±Inf → ±Inf MPFR (ternary 0).
 *        - Subnormal d → renormalised normal MPFR (the implicit-1
 *          promotion in set_d is lossless).
 *        - Normal d → normal MPFR with the exact 53-bit mantissa
 *          (the lossless-pad branch in set_d.ts triggers because
 *          53n >= 53n).
 *
 *      We discard the ternary field; it must be 0 by construction.
 *
 *   4. Delegate to {@link compareMPFR}.
 *
 * Why prec=53 (not higher)
 * ------------------------
 *
 * 53 bits is the IEEE 754 binary64 significand width. A `d` cannot
 * carry more information than 53 mantissa bits regardless of how
 * high a precision the temp MPFR is allocated at — set_d above 53
 * just pads with zeros. So 53 is both necessary (lower would round)
 * and sufficient (higher is wasted allocation).
 *
 * Signed zero handling
 * --------------------
 *
 * mpfr_set_d correctly produces `posZero(53n)` for `+0` and
 * `negZero(53n)` for `-0` (it uses `Object.is(d, -0)`, see
 * src/ops/set_d.ts §"Specials"). compareMPFR's zero-vs-zero branch
 * returns 0 regardless of sign (mpfr/src/cmp.c L57–L58 — signed zero
 * is NOT ordered for cmp), so the cmp_d result against ±0 is the
 * same numeric for either sign of d. This matches the C reference,
 * which also collapses ±0 to 0 in the comparison.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmp_d.c L24–L44 — the C reference.
 *   - src/ops/set_d.ts — the d-to-MPFR exact conversion this port
 *     composes.
 *   - src/internal/mpfr/cmp_raw.ts — the shared comparison core.
 *   - src/ops/cmp.ts — sibling MPFR-vs-MPFR throwing surface.
 *   - mpfr/tests/tcmp_d.c — source for the `mined` cases. Note the
 *     `MPFR_DBL_NAN` test at L86–L104 confirms NaN-d causes the C
 *     erange flag — the TS analogue is the throw branch above.
 *   - CLAUDE.md "Hallucination-risk callouts": NaN ≠ NaN (we throw
 *     on either operand's NaN); Signed zero is real (for arithmetic,
 *     NOT for cmp); rounding-mode count is FIVE (set_d uses RNDN
 *     since the conversion is exact at prec=53).
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';
import { mpfr_set_d } from './set_d.ts';

/**
 * Compare an {@link MPFR} value against an IEEE 754 binary64 (`number`).
 *
 * @param x  MPFR value. Must pass {@link import('../core.ts').validate}.
 * @param d  IEEE 754 double. Must not be NaN.
 * @returns `-1` if `x < d`, `0` if `x == d`, `+1` if `x > d`.
 *
 * @throws {MPFRError} `EDOMAIN` if `x.kind === 'nan'` OR
 *   `Number.isNaN(d)`. The dual-throw matches the C reference's behaviour
 *   (which sets erange + returns 0 for either NaN path); see the module
 *   docstring "Divergence from C → TS".
 * @throws {MPFRError} `EPREC` if `d` is not a JS `number` (defensive
 *   runtime check for callers crossing the type boundary, e.g. JSON-
 *   decoded grader inputs).
 *
 * @mpfrName mpfr_cmp_d
 */
export function mpfr_cmp_d(x: MPFR, d: number): number {
  // --- Boundary validation ---------------------------------------------------
  // The type-boundary check on `d` defends against JSON-decoded values
  // arriving as strings. The grader's value_codec.ts decodeInputValue
  // does the right thing (decodes "NaN" / "+Infinity" / finite "%.17g"
  // strings back to numbers), but a future codec change or a direct
  // caller that bypassed the codec would surface here rather than
  // silently miscomparing.
  if (typeof d !== 'number') {
    throw new MPFRError('EPREC', `d must be number, got ${typeof d}`);
  }

  // --- NaN paths → throw -----------------------------------------------------
  // Two independent NaN guards, both throwing EDOMAIN. We check `d`
  // first because it's a single Number.isNaN call (no MPFR field
  // access); the order has no observable effect, only minor perf.
  if (Number.isNaN(d)) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_d: NaN d (cmp_d requires a non-NaN double operand)`,
    );
  }
  if (x.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_d: NaN x (cmp_d requires a non-NaN MPFR operand)`,
    );
  }

  // --- Build a temp MPFR exact-representation of d ---------------------------
  // mpfr_set_d at prec=53 is EXACT for every non-NaN binary64 (see the
  // module docstring §"Why prec=53"). Signed zero is preserved
  // (Object.is(d, -0) in set_d.ts), which is fine here because
  // compareMPFR treats ±0 as equal anyway (signed zero is not
  // ordered for cmp).
  //
  // We discard `ternary`; the conversion is lossless by construction
  // so ternary === 0. The `value` field is the temp MPFR we hand to
  // compareMPFR.
  const { value: temp } = mpfr_set_d(d, 53n, 'RNDN');

  // --- Delegate to compareMPFR -----------------------------------------------
  const r = compareMPFR(x, temp);
  if (r === null) {
    // Unreachable: x is non-NaN (checked above) and temp is non-NaN
    // (we threw on NaN d before constructing temp). The defensive
    // throw protects against a future schema change.
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp_d: compareMPFR returned null unexpectedly (x.kind=${x.kind}, d=${d})`,
    );
  }
  return r;
}
