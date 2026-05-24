/**
 * ops/cmp.ts — pure-TS port of MPFR's `mpfr_cmp`.
 *
 * Compare two {@link MPFR} values and return a signed comparison result
 * in `{-1, 0, +1}` — `-1` if `a < b`, `0` if `a == b`, `+1` if `a > b`.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp(mpfr_srcptr op1, mpfr_srcptr op2);
 *
 *   Returns the sign of `op1 - op2`. Sets the erange flag and returns 0
 *   if either operand is NaN. See mpfr/src/cmp.c L100–L105: `mpfr_cmp`
 *   is just `mpfr_cmp3(op1, op2, +1)`; the underlying algorithm lives
 *   at mpfr/src/cmp.c L32–L98.
 *
 * TS signature
 * ------------
 *
 *   mpfr_cmp(a: MPFR, b: MPFR): number;
 *
 *   - takes two immutable {@link MPFR} values from src/core.ts;
 *   - returns a plain JS `number` (∈ `{-1, 0, +1}`) — no Result wrapper
 *     because there is no rounding to attribute ternary to.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * MPFR's C surface signals NaN by setting an erange flag and returning
 * 0 — a silent "invalid comparison" channel that callers must remember
 * to test. The idiomatic TS port instead **throws** `MPFRError` with
 * code `'EDOMAIN'` on any NaN operand. The rationale is in CLAUDE.md
 * "Hallucination-risk callouts: NaN ≠ NaN":
 *
 *   > the idiomatic TS port should throw a documented MPFRRangeError,
 *   > never silently return 0.
 *
 * Callers that need the C semantics can wrap in a try/catch; callers
 * that don't get a loud failure instead of a silent wrong-zero. This
 * matches the precedent set by the other ops in `src/ops/` (set_d /
 * get_d / init2 all throw `MPFRError` rather than rely on erange).
 *
 * Algorithm
 * ---------
 *
 * Mirrors mpfr/src/cmp.c L32–L98 step-by-step. The C source structures
 * the dispatch around `MPFR_ARE_SINGULAR` (true if either operand is
 * NaN, Inf, or zero); the TS port flattens this into discrete `kind`
 * checks because the locked schema makes the discriminant explicit.
 *
 *   1. NaN: throw EDOMAIN. (C: erange + return 0; mpfr/src/cmp.c L43–L47.)
 *
 *   2. Both zero (regardless of sign): return 0. Signed zero is NOT
 *      ordered by mpfr_cmp — `+0 == -0` for comparison even though both
 *      are observably distinct under arithmetic. This is a documented
 *      MPFR quirk; see mpfr/src/cmp.c L57–L58:
 *
 *        else if (MPFR_IS_ZERO(b))
 *          return MPFR_IS_ZERO(c) ? 0 : -s;
 *
 *      (s here is +1 for the mpfr_cmp entry; the early-return reads
 *      "zero vs zero is equal, zero vs anything-nonzero is opposite the
 *      sign of the nonzero operand".)
 *
 *   3. Both Inf: return `a.sign - b.sign` clamped to {-1, 0, +1}. So
 *      `+Inf vs -Inf → +1`, `-Inf vs +Inf → -1`, same-sign infinities
 *      return 0. (mpfr/src/cmp.c L48–L54.)
 *
 *   4. One Inf:
 *        - if `a` is Inf: return `a.sign` (Inf magnitude dominates).
 *        - if `b` is Inf: return `-b.sign`.
 *      (mpfr/src/cmp.c L48–L56.)
 *
 *   5. One zero (the other normal):
 *        - if `a` is zero: return `-b.sign` (zero < positive, > negative).
 *        - if `b` is zero: return `a.sign`.
 *      (mpfr/src/cmp.c L57–L60, with `s=+1` substituted.)
 *
 *   6. Both normal:
 *        a. Sign differs: return `a.sign`. (mpfr/src/cmp.c L63–L64.)
 *        b. Same sign, exponent decides:
 *             a.exp > b.exp → +a.sign  (a larger in magnitude)
 *             a.exp < b.exp → -a.sign
 *           (mpfr/src/cmp.c L68–L73.)
 *        c. Sign and exponent both equal: compare mantissas. Because
 *           a.prec and b.prec may differ, we align both to a common
 *           width (max(a.prec, b.prec)) by left-shifting the
 *           lower-prec mantissa. The aligned bigints can then be
 *           compared directly; the sign of `(a_aligned - b_aligned)`
 *           multiplied by `a.sign` is the result.
 *
 *           Why the multiply by `a.sign`: at this point `a.sign === b.sign`,
 *           so the larger-magnitude operand is the one with the larger
 *           aligned mantissa. If both are positive, larger magnitude
 *           means larger value (+1); if both are negative, larger
 *           magnitude means smaller value (-1). That's exactly `a.sign`
 *           applied to the magnitude comparison.
 *
 *           (Mirrors mpfr/src/cmp.c L77–L97 — the limb-by-limb walk
 *           there is just an MSB-aligned multi-limb integer compare; we
 *           do the same in one bigint operation.)
 *
 * Same-value-different-prec
 * -------------------------
 *
 * Because the locked schema MSB-aligns the mantissa to its own `prec`,
 * the same value at two different precisions has two different `mant`
 * bigints. For example, `1.5` at prec=2 has `mant = 0b11 = 3`,
 * `exp=1`, and at prec=53 has `mant = 0b11 << 51 = 3 * 2^51`,
 * `exp=1`. Left-shifting the lower-prec mantissa by `(53 - 2) = 51`
 * bits gives `3 << 51`, equal to the prec=53 mant — so the alignment
 * step is what makes mixed-prec equality observable. The
 * `eval/reference_ports/broken/mpfr_cmp.ts` mutation skips this shift
 * and so reports such pairs as nonzero. Per CLAUDE.md PIL.3, that
 * broken port must score composite ≤ 0.5.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/cmp.c L32–L105 — the C reference (the `mpfr_cmp3` core
 *     and the `mpfr_cmp` entry that fixes `s=+1`).
 *   - src/core.ts — locked schema. The MPFR value model with explicit
 *     `kind` discriminant + signed zero/inf is what makes the dispatch
 *     direct here vs. the C source's bitmask probing.
 *   - src/internal/mpn/cmp.ts — the substrate-level limb compare. Note
 *     the iteration direction is OPPOSITE there (LSB-first vs MSB-first)
 *     because limbs are little-endian by index — though here we work in
 *     bigint, which handles the byte order implicitly.
 *   - CLAUDE.md "Hallucination-risk callouts": "NaN ≠ NaN" (we throw on
 *     NaN); "Signed zero is real" (preserved for arithmetic, but NOT
 *     for cmp — see step 2 above); "Rounding mode count is FIVE" (does
 *     not apply, no rounding here).
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';

/**
 * Compare two {@link MPFR} values. Returns `-1` if `a < b`, `0` if `a
 * == b`, `+1` if `a > b`. Signed zero is **not** ordered: `+0 == -0`
 * for the purposes of this comparison.
 *
 * Implementation delegates the kind/sign/exp/mant dispatch to
 * {@link compareMPFR} in `src/internal/mpfr/cmp_raw.ts` — the shared
 * non-throwing core also used by the predicate family
 * (mpfr_less_p / mpfr_greater_p / mpfr_lessequal_p / mpfr_greaterequal_p
 * / mpfr_equal_p, mpfr/src/comparisons.c L39–L73). The only difference
 * between the two surfaces is NaN handling: `compareMPFR` returns
 * `null`, this throws `MPFRError('EDOMAIN', ...)` — see the file
 * docstring "Divergence from C → TS" for the rationale.
 *
 * @param a left operand. Must pass {@link import('../core.ts').validate}.
 * @param b right operand. Must pass {@link import('../core.ts').validate}.
 * @returns `-1`, `0`, or `+1`.
 * @throws {MPFRError} `EDOMAIN` if either operand has `kind === 'nan'`.
 *   This diverges from the C reference, which sets the erange flag and
 *   returns 0 — see the file docstring "Divergence from C → TS".
 * @throws {MPFRError} `EPREC` if either operand fails structural
 *   validation (malformed shape; should not occur for values returned
 *   by other ops in this library).
 *
 * @mpfrName mpfr_cmp
 */
export function mpfr_cmp(a: MPFR, b: MPFR): number {
  // Delegate to the shared core. `null` is the NaN sentinel; translate
  // to an EDOMAIN throw to preserve mpfr_cmp's documented divergence
  // from the C reference (C returns 0 + sets erange; idiomatic TS
  // throws, per CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN").
  const r = compareMPFR(a, b);
  if (r === null) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_cmp: NaN operand (a.kind=${a.kind}, b.kind=${b.kind})`,
    );
  }
  return r;
}
