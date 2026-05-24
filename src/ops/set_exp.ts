/**
 * ops/set_exp.ts — pure-TS port of MPFR's `mpfr_set_exp`.
 *
 * Set the unbiased base-2 exponent of a normal {@link MPFR} value.
 *
 * C signature
 * -----------
 *
 *   int mpfr_set_exp(mpfr_ptr x, mpfr_exp_t exponent);
 *
 *   Body (mpfr/src/set_exp.c L24-L38):
 *
 *     int mpfr_set_exp (mpfr_ptr x, mpfr_exp_t exponent) {
 *       if (MPFR_LIKELY (MPFR_IS_PURE_FP (x) &&
 *                        exponent >= __gmpfr_emin &&
 *                        exponent <= __gmpfr_emax))
 *         {
 *           MPFR_EXP(x) = exponent;
 *           return 0;
 *         }
 *       else
 *         {
 *           return 1;
 *         }
 *     }
 *
 *   `MPFR_IS_PURE_FP` is true iff the value is a normal (non-singular)
 *   floating-point number — i.e. not NaN, ±∞, or ±0.
 *
 *   `__gmpfr_emin` and `__gmpfr_emax` are thread-local exponent bounds;
 *   by default they equal `MPFR_EMIN_DEFAULT` / `MPFR_EMAX_DEFAULT`.
 *
 *   Ref: mpfr/src/set_exp.c L24-L38 — the C reference body.
 *   Ref: mpfr/src/mpfr.h L231-L232 — MPFR_EMAX_DEFAULT = 2^30 - 1,
 *        MPFR_EMIN_DEFAULT = -(MPFR_EMAX_DEFAULT).
 *
 * TS signature
 * ------------
 *
 *   mpfr_set_exp(x: MPFR, e: bigint): MPFR
 *
 *   - For `kind === 'normal'` and `e ∈ [EMIN_DEFAULT, EMAX_DEFAULT]`:
 *     returns a fresh `MPFR` with `exp` replaced by `e`; all other
 *     fields (`kind`, `sign`, `prec`, `mant`) are unchanged.
 *   - Otherwise: throws `MPFRError('EDOMAIN', ...)`.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The C version mutates `x` in place and returns `0` (success) or `1`
 * (failure). The TS port is immutable: it returns a fresh `MPFR` on
 * success and throws `MPFRError('EDOMAIN')` on failure. The `EDOMAIN`
 * discriminant is consistent with the `mpfr_get_exp` sister port
 * (src/ops/get_exp.ts) which uses the same throw-not-signal convention
 * for non-normal inputs.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_exp.c L24-L38 — the C reference implementation.
 *   - mpfr/src/mpfr.h L231-L232 — MPFR_EMAX_DEFAULT / MPFR_EMIN_DEFAULT.
 *   - src/core.ts §MPFRError, EDOMAIN — domain-error for bad inputs.
 *   - src/ops/get_exp.ts — sister port; same EDOMAIN throw convention.
 *   - CLAUDE.md "Hallucination-risk callouts: Subnormals" — emax/emin are
 *     explicit in MPFR; we use the defaults here.
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Default exponent bounds, matching MPFR_EMAX_DEFAULT and MPFR_EMIN_DEFAULT.
 *
 * Ref: mpfr/src/mpfr.h L231-L232 — `#define MPFR_EMAX_DEFAULT ((mpfr_exp_t)
 *   (((mpfr_uexp_t) 1 << 30) - 1))` and `MPFR_EMIN_DEFAULT = -(EMAX_DEFAULT)`.
 */
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n; // 1073741823n
const EMIN_DEFAULT: bigint = -EMAX_DEFAULT;    // -1073741823n

/**
 * Return a fresh {@link MPFR} with the base-2 exponent replaced by `e`.
 *
 * @mpfrName mpfr_set_exp
 *
 * @param x   The source {@link MPFR} value. Must be `kind === 'normal'`.
 * @param e   The new unbiased base-2 exponent. Must be in
 *            `[EMIN_DEFAULT, EMAX_DEFAULT]` = `[-(2^30-1), 2^30-1]`.
 *
 * @returns   A fresh `MPFR` equal to `{ ...x, exp: e }` — same precision,
 *            mantissa, sign, and kind; only the exponent is replaced.
 *
 * @throws {MPFRError} `EDOMAIN` if `x.kind !== 'normal'` (i.e. zero, inf,
 *                    or nan) or if `e` is outside `[EMIN_DEFAULT, EMAX_DEFAULT]`.
 *
 * @example
 *   const x = setD(1.5, 53n, 'RNDN').value;  // exp=1n
 *   mpfr_set_exp(x, 5n).exp;                 // 5n
 *   mpfr_set_exp(posZero(53n), 0n);           // throws EDOMAIN
 *   mpfr_set_exp(x, 2n ** 30n);              // throws EDOMAIN (out of range)
 */
export function mpfr_set_exp(x: MPFR, e: bigint): MPFR {
  // Ref: mpfr/src/set_exp.c L26 — MPFR_IS_PURE_FP(x): x must be a normal
  //   (non-singular) FP value. NaN, ±Inf, and ±0 are all rejected.
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_exp: x must be a normal (pure FP) value, got kind='${x.kind}'`,
    );
  }

  // Ref: mpfr/src/set_exp.c L27-L28 — exponent >= __gmpfr_emin &&
  //   exponent <= __gmpfr_emax (using the default bounds from mpfr.h L231-L232).
  if (e < EMIN_DEFAULT || e > EMAX_DEFAULT) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_set_exp: exponent ${e} out of range [${EMIN_DEFAULT}, ${EMAX_DEFAULT}]`,
    );
  }

  // Ref: mpfr/src/set_exp.c L30 — MPFR_EXP(x) = exponent.
  //   C mutates in place; TS returns a fresh immutable record.
  //   All fields other than `exp` are preserved unchanged.
  return {
    kind: x.kind,
    sign: x.sign,
    prec: x.prec,
    exp: e,
    mant: x.mant,
  };
}
