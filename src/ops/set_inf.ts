/**
 * ops/set_inf.ts — pure-TS port of MPFR's `mpfr_set_inf`.
 *
 * Public-surface op. Imports from the locked schema in `src/core.ts`
 * per CLAUDE.md Law 4. Constructs a signed-infinity MPFR at the
 * requested precision.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_inf(mpfr_ptr x, int sign);
 *
 *   - mutates `x` in place to ±Inf at `x`'s pre-existing precision;
 *   - sets sign per the rule: `sign >= 0` → `+Inf`, `sign < 0` → `-Inf`.
 *
 *   Ref: mpfr/src/set_inf.c L24–L32. The C body is
 *
 *     MPFR_SET_INF(x);
 *     if (sign >= 0) MPFR_SET_POS(x);
 *     else           MPFR_SET_NEG(x);
 *
 *   so the C semantic is identical in shape to `mpfr_set_zero`:
 *   non-negative → +, strictly negative → −.
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_inf(prec: bigint, sign: 1 | -1): MPFR;
 *
 *   - takes `prec` and `sign` as explicit positional arguments;
 *   - returns the bare `MPFR` (no `Result` wrapper — there's no
 *     rounding step, hence no ternary to report).
 *
 * The TS surface restricts `sign` to the strict `Sign` type
 * (`1 | -1`) — same reasoning as `mpfr_set_zero`: the C silent-
 * coerce-on-zero rule is the wrong default; the strict shape forces
 * callers to commit. A `sign === 0` (or any other value) at runtime
 * throws `MPFRError(EPREC)`.
 *
 * Why a separate prec argument
 * ----------------------------
 *
 * The C `mpfr_set_inf(x, sign)` takes the precision implicitly from
 * `x` (the slot being mutated). The immutable TS schema preserves the
 * precision on the `inf` MPFR value (see `posInf`/`negInf` factories
 * in src/core.ts L258–L271 — both take `prec: bigint`), because
 * downstream ops *do* read the precision off an infinity value
 * (e.g. `mpfr_add(+inf, x, prec, rnd)` consumes the explicit `prec`
 * argument but propagates the kind-tag from either operand). The
 * factory enforces the bounds check; we thread `prec` through.
 *
 * Algorithm
 * ---------
 *
 * Trivial: dispatch on `sign`, delegate to `posInf(prec)` or
 * `negInf(prec)`. The factory does the prec validation (assertPrec
 * inside core.ts L445–L455); we add a `sign` validation here so a
 * malformed runtime input gets a clear diagnostic instead of falling
 * through to the wrong factory.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_inf.c — the C reference.
 *   - src/core.ts L258–L271 — `posInf` / `negInf` constructors.
 *   - src/core.ts L75–L90 — `Sign` discriminant.
 *   - CLAUDE.md Law 4 — library coherence.
 */

import type { MPFR, Sign } from '../core.ts';
import { MPFRError, negInf, posInf } from '../core.ts';

/**
 * Construct a signed-infinity MPFR at the requested precision.
 *
 * @mpfrName mpfr_set_inf
 *
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 *              Passed through to `posInf` / `negInf` for the
 *              centralised bounds check.
 * @param sign  `1` for `+Inf`, `-1` for `-Inf`. Strictly typed;
 *              runtime callers passing anything else hit `EPREC`.
 *
 * @returns     an `MPFR` value with `kind === 'inf'`, the requested
 *              `sign`, `prec === prec`, `exp === 0n`, `mant === 0n`.
 *              Passes `validate()` without post-processing.
 *
 * @throws {MPFRError} `EPREC` when `prec` is out of range (propagated
 *                    from `posInf` / `negInf`) OR when `sign` is not
 *                    one of `1` / `-1`.
 *
 * @example
 *   mpfr_set_inf(53n, 1);   // +Inf at IEEE float64 precision
 *   mpfr_set_inf(53n, -1);  // -Inf at IEEE float64 precision
 */
export function mpfr_set_inf(prec: bigint, sign: Sign): MPFR {
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_set_inf: sign must be 1 or -1, got ${String(sign)}`,
    );
  }
  return sign === 1 ? posInf(prec) : negInf(prec);
}
