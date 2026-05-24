/**
 * ops/set_zero.ts — pure-TS port of MPFR's `mpfr_set_zero`.
 *
 * Public-surface op. Imports from the locked schema in `src/core.ts`
 * per CLAUDE.md Law 4. Constructs a signed-zero MPFR at the requested
 * precision; signed zero is observable in MPFR rounding (CLAUDE.md
 * "Signed zero is real"), so `+0` and `-0` are distinct return values.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_zero(mpfr_ptr x, int sign);
 *
 *   - mutates `x` in place to ±0 at `x`'s pre-existing precision;
 *   - sets sign per the rule: `sign >= 0` → `+0`, `sign < 0` → `-0`.
 *
 *   Ref: mpfr/src/set_zero.c L24–L30. The C body is
 *
 *     mpfr_set_ui(x, 0, MPFR_RNDN);   // x = +0 at x's prec
 *     if (sign < 0) MPFR_SET_NEG(x);  // flip sign if requested
 *
 *   so the C semantic is "non-negative int → +0, strictly negative int
 *   → -0". The MPFR convention `+1 / -1` for the public API is the
 *   conventional positional encoding; both `0` and `+1` map to +0.
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_zero(prec: bigint, sign: 1 | -1): MPFR;
 *
 *   - takes `prec` and `sign` as explicit positional arguments;
 *   - returns the bare `MPFR` (no `Result` wrapper — there's no
 *     rounding step, hence no ternary to report).
 *
 * The TS surface restricts `sign` to the strict `Sign` type
 * (`1 | -1`) — there is no "non-negative int → +0" coercion as in the
 * C version because the immutable schema's `Sign` is binary. Callers
 * who want to pass an integer (e.g. from a comparison) must narrow
 * it first; mismatched runtime input (`sign === 0`, `sign === 2`,
 * a string, etc.) throws `MPFRError(EPREC)`.
 *
 * Why throw rather than silently coerce? The C `if (sign < 0)` rule
 * is sloppy by modern standards — it silently treats any positive int
 * or zero as +0, which is the wrong default for a port whose entire
 * job is to make the contract explicit. The strict-`Sign` shape forces
 * callers to commit to a sign at the call site; a future port that
 * needs the C-style coercion can do it locally and pass the resulting
 * `1 | -1`.
 *
 * Algorithm
 * ---------
 *
 * Trivial: dispatch on `sign`, delegate to `posZero(prec)` or
 * `negZero(prec)`. The factory does the prec validation (assertPrec
 * inside core.ts L445–L455); we add a `sign` validation here so a
 * `sign === 0` runtime input gets a clear `MPFRError` rather than
 * falling through to the (wrong) `posZero` path.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_zero.c — the C reference.
 *   - src/core.ts L278–L292 — `posZero` / `negZero` constructors.
 *   - src/core.ts L75–L90 — `Sign` discriminant (observable on zero).
 *   - CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *     `+0` and `-0` are distinct MPFR values; the sign must survive
 *     through every op that handles them.
 *   - CLAUDE.md Law 4 — library coherence: public ports must import
 *     the locked schema.
 */

import type { MPFR, Sign } from '../core.ts';
import { MPFRError, negZero, posZero } from '../core.ts';

/**
 * Construct a signed-zero MPFR at the requested precision.
 *
 * @mpfrName mpfr_set_zero
 *
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 *              Passed through to `posZero` / `negZero` for the
 *              centralised bounds check.
 * @param sign  `1` for `+0`, `-1` for `-0`. Strictly typed; runtime
 *              callers passing anything else hit the `EPREC` branch.
 *
 * @returns     an `MPFR` value with `kind === 'zero'`, the requested
 *              `sign`, `prec === prec`, `exp === 0n`, `mant === 0n`.
 *              Passes `validate()` without post-processing.
 *
 * @throws {MPFRError} `EPREC` when `prec` is out of range (propagated
 *                    from `posZero`/`negZero`) OR when `sign` is not
 *                    one of `1` / `-1`.
 *
 * @example
 *   mpfr_set_zero(53n, 1);   // +0 at IEEE float64 precision
 *   mpfr_set_zero(53n, -1);  // -0 at IEEE float64 precision
 */
export function mpfr_set_zero(prec: bigint, sign: Sign): MPFR {
  // Sign validation. Strict `1 | -1`; reject everything else with a
  // precise diagnostic. We use `EPREC` (rather than inventing a new
  // code) because the existing `MPFRErrorCode` enum is closed (src/
  // core.ts L187) and "bad input" is the umbrella the schema uses for
  // structural-shape violations.
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_set_zero: sign must be 1 or -1, got ${String(sign)}`,
    );
  }
  // Delegate to the factory; prec validation lives there (assertPrec).
  return sign === 1 ? posZero(prec) : negZero(prec);
}
