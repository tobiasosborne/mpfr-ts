/**
 * ops/setmax.ts — pure-TS port of MPFR's `mpfr_setmax`.
 *
 * Construct the maximum representable MPFR at a given precision and
 * exponent: the value whose mantissa has all `prec` bits set to 1 —
 * i.e. `mant = (1n << prec) - 1n` which is `2^prec - 1`.
 *
 * C signature
 * -----------
 *
 *   void mpfr_setmax(mpfr_ptr x, mpfr_exp_t e);
 *
 *   Body (mpfr/src/setmax.c L26-L40):
 *
 *     MPFR_SET_EXP(x, e);                                    // exp ← e
 *     xn = MPFR_LIMB_SIZE(x);                                // # limbs
 *     sh = (mpfr_prec_t) xn * GMP_NUMB_BITS - MPFR_PREC(x); // trailing zero bits
 *     xp = MPFR_MANT(x);
 *     xp[0] = MPFR_LIMB_MAX << sh;                          // low limb, zero the trailing sh bits
 *     for (i = 1; i < xn; i++)
 *       xp[i] = MPFR_LIMB_MAX;                              // all other limbs all-ones
 *
 *   Net effect: exactly the high `prec` bits set across the limb array,
 *   which in the TS bigint representation is `mant = 2^prec - 1`. The
 *   magnitude (via value formula `sign * mant * 2^(exp - prec)`) is
 *   `(2^prec - 1) * 2^(exp - prec)` = `2^exp - 2^(exp - prec)`.
 *
 *   Ref: mpfr/src/setmax.c L26-L40 — the C reference.
 *   Ref: mpfr/src/mpfr-impl.h L1301-L1302 — MPFR_LIMB_MAX = all bits set.
 *
 * TS signature
 * ------------
 *
 *   mpfr_setmax(prec: bigint, exp: bigint, sign?: Sign): MPFR;
 *
 *   - `prec` must be in `[PREC_MIN, PREC_MAX]`.
 *   - `exp` can be any bigint (no emax range check here; compose with
 *     a downstream `check_range` op if needed).
 *   - `sign` defaults to `+1`, matching the most common caller pattern
 *     (the C comment "current sign is kept" means the caller sets sign
 *     before calling setmax; TS defaults to positive).
 *
 * Why no Result wrapper
 * ---------------------
 *
 * The construction is exact by definition — there is no rounding step
 * and hence no ternary to report. Matches the convention shared with
 * `mpfr_setmin`, `mpfr_set_inf`, `mpfr_set_zero`, etc.
 *
 * Algorithm
 * ---------
 *
 *   1. Validate inputs (prec in range, exp is bigint, sign is 1 or -1).
 *   2. Compute `mant = (1n << prec) - 1n` — all prec bits set, equivalent
 *      to the C limb-fill with MPFR_LIMB_MAX.
 *   3. Return `{kind: 'normal', sign, prec, exp, mant}`.
 *
 * Hallucination guards (from CLAUDE.md)
 * --------------------------------------
 *
 *   - prec is in BITS, not decimal digits.
 *   - The all-ones mantissa is `2^prec - 1`, NOT `2^prec` (out of range)
 *     or `2^(prec-1)` (that's setmin, the minimum, not the maximum).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/setmax.c L26-L40 — the C reference.
 *   - mpfr/src/mpfr-impl.h L1301-L1302 — MPFR_LIMB_HIGHBIT, MPFR_LIMB_MAX.
 *   - src/core.ts L93-L135 — MPFR value model; mantissa MSB-alignment.
 *   - src/core.ts L51-L62 — value formula.
 *   - src/ops/setmin.ts — sibling constructor; identical structure modulo
 *     the mantissa formula (1n << (prec-1n) vs (1n << prec) - 1n).
 *   - mpfr/tests/tfma.c L90, L123 — typical caller pattern.
 *   - CLAUDE.md Law 4 — library coherence.
 */

import type { MPFR, Sign } from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '/home/tobias/Projects/mpfr-ts/src/core.ts';

/**
 * Validate the public-boundary scalar arguments. Mirrors setmin's gate.
 */
function validateArgs(prec: bigint, exp: bigint, sign: Sign): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (typeof exp !== 'bigint') {
    throw new MPFRError('EPREC', `exp must be bigint, got ${typeof exp}`);
  }
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_setmax: sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

/**
 * Construct the maximum representable normal MPFR at the given precision
 * and exponent.
 *
 * @mpfrName mpfr_setmax
 *
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param exp   unbiased base-2 exponent. Any bigint.
 * @param sign  optional sign (defaults to `+1`).
 *
 * @returns     an `MPFR` value with `kind === 'normal'`, the requested
 *              `sign` / `prec` / `exp`, and `mant === (1n << prec) - 1n`
 *              (all prec bits set). Magnitude is `2^exp - 2^(exp - prec)`.
 *              Passes `validate()` without post-processing.
 *
 * @throws {MPFRError} `EPREC` on bad precision, non-bigint exp, or
 *                    non-`Sign` sign.
 *
 * @example
 *   const x = mpfr_setmax(53n, 1024n);
 *   // x represents the largest double-precision normal value with
 *   // exponent 1024: (2^53 - 1) * 2^(1024 - 53) = 2^1024 - 2^971
 *
 *   const negMax = mpfr_setmax(53n, 1024n, -1);
 *   // Same magnitude with negative sign.
 */
export function mpfr_setmax(
  prec: bigint,
  exp: bigint,
  sign: Sign = 1,
): MPFR {
  validateArgs(prec, exp, sign);

  // Mantissa is `(1n << prec) - 1n`: all prec bits set to 1.
  // This collapses the C limb-fill (MPFR_LIMB_MAX for all limbs,
  // with low limb shifted to zero trailing bits) into a single bigint
  // expression. The MSB-alignment invariant holds: `mant === 2^prec - 1`
  // means bit `prec - 1` is set (the MSB is set) and no bit at position
  // `>= prec` is set.
  //
  // Ref: mpfr/src/setmax.c L26-L40 — xp[0] = MPFR_LIMB_MAX << sh,
  //   xp[i] = MPFR_LIMB_MAX for i >= 1.
  const mant = (1n << prec) - 1n;

  return {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  };
}
