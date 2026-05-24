/**
 * ops/setmin.ts — pure-TS port of MPFR's `mpfr_setmin`.
 *
 * Construct the minimum representable MPFR at a given precision and
 * exponent: the value whose mantissa is exactly `2^(prec-1)` — the MSB
 * set, all lower bits zero. This is the smallest normal representable
 * value at the given precision and exponent; its magnitude is
 * `2^(prec-1) * 2^(exp - prec)` = `2^(exp - 1)`.
 *
 * C signature
 * -----------
 *
 *   void mpfr_setmin(mpfr_ptr x, mpfr_exp_t e);
 *
 *   Body (mpfr/src/setmin.c L27-L37):
 *
 *     MPFR_SET_EXP(x, e);                      // exp ← e
 *     xn = (MPFR_PREC(x) - 1) / GMP_NUMB_BITS; // index of top limb
 *     xp = MPFR_MANT(x);
 *     xp[xn] = MPFR_LIMB_HIGHBIT;              // top limb: MSB only
 *     MPN_ZERO(xp, xn);                        // lower limbs: all zero
 *
 *   Net effect: only the MSB of the mantissa limb array is set. In
 *   MPFR's value formula (mpfr.h L268-L272), with `_d[xn] =
 *   MPFR_LIMB_HIGHBIT` (= 1 << (GMP_NUMB_BITS-1)) and all other limbs
 *   zero, the top limb contributes `MPFR_LIMB_HIGHBIT / 2^GMP_NUMB_BITS`
 *   = 1/2 to the significand. Combined with `exp = e`, the value is
 *   `sign * (1/2) * 2^e` = `sign * 2^(e-1)`.
 *
 *   In the TS locked-schema value formula `sign * mant * 2^(exp - prec)`,
 *   the same magnitude requires `mant * 2^(exp - prec) = 2^(exp - 1)`,
 *   i.e. `mant = 2^(prec - 1)` = `1n << (prec - 1n)`.
 *
 * TS signature
 * ------------
 *
 *   mpfr_setmin(prec: bigint, exp: bigint, sign?: Sign): MPFR;
 *
 *   - `prec`: precision in bits, in `[PREC_MIN, PREC_MAX]`.
 *   - `exp`:  unbiased base-2 exponent (any bigint).
 *   - `sign`: optional sign (defaults to `+1`); the C semantic is
 *             "current sign is kept" (mpfr/src/setmin.c L24 comment).
 *             The TS port defaults to `+1` matching the sibling setmax.
 *
 *   - Returns a bare `MPFR` (no `Result` wrapper — the construction is
 *     exact: no rounding occurs, ternary would be vacuous 0).
 *
 * Algorithm
 * ---------
 *
 *   1. Validate inputs (`prec` in range; `sign` strict `1 | -1`;
 *      `exp` is any bigint, no library-side range constraint).
 *   2. Compute `mant = 1n << (prec - 1n)` — exactly `2^(prec-1)`, the
 *      MSB-aligned single-bit mantissa. The single bigint shift replaces
 *      the C body's `xp[xn] = MPFR_LIMB_HIGHBIT; MPN_ZERO(xp, xn)`.
 *   3. Return `{kind: 'normal', sign, prec, exp, mant}`.
 *
 * Distinction from setmax
 * -----------------------
 *
 *   - setmax: `mant = (1n << prec) - 1n`    (all prec bits set)
 *   - setmin: `mant = 1n << (prec - 1n)`    (only MSB set)
 *
 *   At prec=1, both produce `mant = 1n` (which equals both `1n << 0n` and
 *   `(1n << 1n) - 1n = 1n`). For prec >= 2 they differ.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/setmin.c L27-L37 — the C reference.
 *   - mpfr/src/mpfr-impl.h L1301 — `MPFR_LIMB_HIGHBIT = 1 << (GMP_NUMB_BITS - 1)`.
 *   - mpfr/src/mpfr-impl.h L1071 — `MPFR_SET_EXP` asserts `MPFR_EXP_IN_RANGE`.
 *   - src/core.ts L93-L135 — `MPFR` value model; mantissa MSB-alignment rule.
 *   - src/core.ts L51-L62 — value formula `sign * mant * 2^(exp - prec)`.
 *   - src/ops/setmax.ts — sibling constructor; identical structure modulo mantissa.
 *   - CLAUDE.md "Hallucination-risk callouts: mpfr_prec_t is in bits, not decimal
 *     digits. The MSB-only mantissa is 2^(prec-1), NOT 2^prec or 1."
 */

import type { MPFR, Sign } from '/home/tobias/Projects/mpfr-ts/src/core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '/home/tobias/Projects/mpfr-ts/src/core.ts';

/**
 * Validate the public-boundary scalar arguments. Same convention as
 * setmax.ts / neg.ts. Exp is any bigint (the locked schema imposes
 * no upper bound on exponent magnitude).
 *
 * Ref: mpfr/src/setmin.c L27-L37 — no explicit range check on `e` beyond
 *   the MPFR_SET_EXP macro's internal MPFR_EXP_IN_RANGE assertion; the TS
 *   port mirrors this by imposing no library-side exp bound.
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
      `mpfr_setmin: sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

/**
 * Construct the minimum representable MPFR at the given precision and
 * exponent.
 *
 * @mpfrName mpfr_setmin
 *
 * @param prec  precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param exp   unbiased base-2 exponent. Any bigint — no library-side
 *              range constraint (libmpfr's MPFR_EMIN is not mirrored
 *              into the locked schema).
 * @param sign  optional sign (defaults to `+1`). Strict `1 | -1`.
 *
 * @returns     an `MPFR` value with `kind === 'normal'`, the requested
 *              `sign` / `prec` / `exp`, and `mant === 2^(prec-1)` (the
 *              single MSB-only mantissa). The value's magnitude is
 *              `2^(prec-1) * 2^(exp - prec)` = `2^(exp - 1)`.
 *              Passes `validate()` without post-processing.
 *
 * @throws {MPFRError} `EPREC` on bad precision, non-bigint exp, or
 *                    non-`Sign` sign.
 *
 * @example
 *   const x = mpfr_setmin(53n, 1n);
 *   // x is the smallest representable double-precision value at exp=1:
 *   // mant = 2^52, value = 2^52 * 2^(1-53) = 2^0 = 1.0 exactly.
 *
 *   const negMin = mpfr_setmin(53n, 1n, -1);
 *   // Same magnitude with negative sign: -1.0.
 */
export function mpfr_setmin(
  prec: bigint,
  exp: bigint,
  sign: Sign = 1,
): MPFR {
  validateArgs(prec, exp, sign);

  // Ref: mpfr/src/setmin.c L27-L37 — the C body sets xp[xn] = MPFR_LIMB_HIGHBIT
  //   (the most-significant bit of the top limb) and zeros all lower limbs.
  //   In the TS schema, `mant` is a single bigint MSB-aligned to `prec` bits.
  //   A mantissa with only the MSB set is `1 << (prec - 1)`, which satisfies
  //   the locked-schema invariant `2^(prec-1) <= mant < 2^prec`.
  //
  //   Ref: src/core.ts L93-L135 — MSB-alignment rule: bit prec-1 must be set,
  //   no bits at position >= prec.
  const mant = 1n << (prec - 1n);

  return {
    kind: 'normal',
    sign,
    prec,
    exp,
    mant,
  };
}
