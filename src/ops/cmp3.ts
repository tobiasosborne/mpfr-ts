/**
 * ops/cmp3.ts — pure-TS port of MPFR's `mpfr_cmp3`.
 *
 * Compare `b` against `sign(s) * c` — that is, flip `c`'s sign when `s` is
 * negative, then perform a standard MPFR comparison. Returns a positive
 * value if `b > sign(s)*c`, 0 if equal, a negative value if `b < sign(s)*c`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp3(mpfr_srcptr b, mpfr_srcptr c, int s);
 *
 * The C reference starts with:
 *
 *   s = MPFR_MULT_SIGN(s, MPFR_SIGN(c));   // s now = sign(s) * sign(c)
 *
 * and then uses `s` as the "effective sign" of `c` throughout the dispatch.
 * Ref: mpfr/src/cmp.c L32–L98.
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmp3(b: MPFR, c: MPFR, s: number): number;
 *
 * Implementation strategy
 * -----------------------
 *
 * We use approach (a) from spec.json: create a temporary `c2` whose sign
 * is flipped when `s < 0`, then delegate to `compareMPFR(b, c2)`.
 *
 *   - When `s > 0`: `sign(s)*c == c`, so `c2 = c`.
 *   - When `s < 0`: `sign(s)*c == -c`, so `c2 = { ...c, sign: -c.sign }`.
 *   - When `s == 0`: undefined per C's MPFR_MULT_SIGN semantics (0 is not
 *     a valid sign). We treat it as `+1` to be safe (no golden case uses 0).
 *
 * This delegates all the singular/normal dispatch to `compareMPFR`, which
 * already faithfully mirrors mpfr/src/cmp.c L32–L98.
 *
 * NaN handling
 * ------------
 *
 * The C reference sets the erange flag and returns 0 on NaN. Following
 * the established mpfr_cmp precedent (src/ops/cmp.ts §"Divergence from
 * C → TS"), we throw `MPFRError('EDOMAIN', ...)` instead. The golden
 * master was generated with this behaviour.
 * Ref: CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN".
 *
 * Relationship to mpfr_cmp
 * ------------------------
 *
 *   mpfr_cmp(b, c) ≡ mpfr_cmp3(b, c, +1)
 *
 * Ref: mpfr/src/cmp.c L100–L105.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmp.c L32–L98 — the C reference body.
 *   - mpfr/src/cmp.c L100–L105 — mpfr_cmp as a special case.
 *   - src/internal/mpfr/cmp_raw.ts — non-throwing compare core.
 *   - src/ops/cmp.ts — public throwing mpfr_cmp surface (s=+1 fixed).
 *   - CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN, Signed zero is real".
 */

import type { MPFR } from "../core.ts";
import { MPFRError } from "../core.ts";
import { compareMPFR } from "../internal/mpfr/cmp_raw.ts";

/**
 * Compare `b` against `sign(s) * c`.
 *
 * Returns a positive number if `b > sign(s)*c`, 0 if equal, a negative
 * number if `b < sign(s)*c`. The returned value is normalised to `{-1, 0, +1}`.
 *
 * @param b left operand.
 * @param c right operand before sign-multiplication.
 * @param s sign multiplier; only `Math.sign(s)` is used. Pass `+1` for a
 *   plain comparison (equivalent to `mpfr_cmp`); pass `-1` to negate `c`
 *   before comparing.
 * @returns `-1`, `0`, or `+1`.
 * @throws {MPFRError} `EDOMAIN` if either operand has `kind === 'nan'`.
 *   This diverges from the C reference (which sets the erange flag and
 *   returns 0) — see src/ops/cmp.ts §"Divergence from C → TS".
 *
 * @mpfrName mpfr_cmp3
 */
export function mpfr_cmp3(b: MPFR, c: MPFR, s: number): number {
  // NaN guard — throw EDOMAIN rather than silently return 0 + set erange.
  // The C reference (mpfr/src/cmp.c L43–L47) checks NaN first, inside the
  // MPFR_ARE_SINGULAR block. We check here before any sign manipulation.
  // Ref: CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN".
  if (b.kind === 'nan' || c.kind === 'nan') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp3: NaN operand (b.kind=${b.kind}, c.kind=${c.kind})`,
    );
  }

  // Determine the effective sign multiplier for c. The C code does:
  //   s = MPFR_MULT_SIGN(s, MPFR_SIGN(c))
  // which multiplies s (as an int encoding sign) by c's sign. In TS we
  // only need to know whether the net sign is negative to decide whether
  // to flip c. We use Math.sign(s) to normalise s to {-1, 0, +1} —
  // s=0 is undefined behaviour in C (MPFR_MULT_SIGN treats signs as ints
  // and 0 is not a valid sign value), but we default-treat it as +1.
  // Ref: mpfr/src/cmp.c L39 — MPFR_MULT_SIGN(s, MPFR_SIGN(c)).
  const sSign = Math.sign(s) === -1 ? -1 : 1;

  // Build the effective c: negate its sign if sSign < 0.
  // When sSign === +1, c_eff === c (no allocation needed).
  // When sSign === -1, we create a minimal object literal with the flipped
  // sign — the other fields are unchanged, and the comparison logic uses
  // only kind, sign, exp, mant, prec.
  const cEff: MPFR =
    sSign === -1
      ? { kind: c.kind, sign: (c.sign === 1 ? -1 : 1) as 1 | -1, prec: c.prec, exp: c.exp, mant: c.mant }
      : c;

  // Delegate to the shared non-throwing core. `null` is the NaN sentinel;
  // we've already thrown above, so `null` should be unreachable here.
  // Guard defensively — if `compareMPFR` ever returns null for a non-NaN
  // pair (internal invariant violation), surface it rather than silently
  // returning 0.
  // Ref: src/internal/mpfr/cmp_raw.ts — mirrors mpfr/src/cmp.c L32–L98.
  const r = compareMPFR(b, cEff);
  if (r === null) {
    // Unreachable: we already threw EDOMAIN above for NaN inputs, and
    // compareMPFR only returns null for NaN. If we reach here, something
    // is wrong with the invariant; surface it explicitly.
    throw new MPFRError(
      'EDOMAIN',
      'mpfr_cmp3: internal invariant violated — compareMPFR returned null for non-NaN pair',
    );
  }
  return r;
}
