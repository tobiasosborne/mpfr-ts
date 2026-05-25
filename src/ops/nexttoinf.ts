/**
 * ops/nexttoinf.ts -- pure-TS port of MPFR's `mpfr_nexttoinf`.
 *
 * Step `x` one ULP in the direction of +/-infinity (preserving the sign
 * of x), at x's own precision. There is no rounding (the "next" value is
 * unique in the precision-defined lattice), so the TS surface returns a
 * fresh {@link MPFR} rather than the canonical `Result {value, ternary}`
 * shape used by rounding ops.
 *
 * C signature
 * -----------
 *
 *   void mpfr_nexttoinf(mpfr_ptr x);
 *
 * Body (mpfr/src/next.c L87-L117):
 *
 *   void mpfr_nexttoinf (mpfr_ptr x) {
 *     if (MPFR_UNLIKELY (MPFR_IS_SINGULAR (x))) {
 *       if (MPFR_IS_ZERO (x))
 *         mpfr_setmin (x, __gmpfr_emin);
 *     } else {
 *       mp_size_t xn;
 *       int sh;
 *       mp_limb_t *xp;
 *       xn = MPFR_LIMB_SIZE (x);
 *       MPFR_UNSIGNED_MINUS_MODULO (sh, MPFR_PREC(x));
 *       xp = MPFR_MANT(x);
 *       if (MPFR_UNLIKELY (mpn_add_1 (xp, xp, xn, MPFR_LIMB_ONE << sh))) {
 *         mpfr_exp_t exp = MPFR_EXP (x);
 *         if (MPFR_UNLIKELY (exp == __gmpfr_emax))
 *           MPFR_SET_INF (x);
 *         else {
 *           MPFR_SET_EXP (x, exp + 1);
 *           xp[xn-1] = MPFR_LIMB_HIGHBIT;
 *         }
 *       }
 *     }
 *   }
 *
 * TS signature
 * ------------
 *
 *   mpfr_nexttoinf(x: MPFR): MPFR;
 *
 * Algorithm in the TS schema
 * --------------------------
 *
 * The TS mantissa is MSB-aligned to exactly `prec` bits (no limb-padding),
 * so the C's "add MPFR_LIMB_ONE << sh" collapses to `mant += 1n` and the
 * carry-out condition becomes `mant >= 2^prec`. Cases:
 *
 *   1. NaN: propagate. The C falls through silently (the singular branch
 *      only acts on zero), leaving NaN unchanged. The TS port mirrors
 *      this: NaN -> NAN_VALUE.
 *
 *   2. +/-Inf: no-op (already at the destination). The C falls through
 *      with no action; the TS port returns x unchanged (modulo schema:
 *      validate(x) would already have approved on input, so returning x
 *      is safe and incurs no allocation).
 *
 *   3. +/-0: emit `mpfr_setmin` at emin: SAME sign (no flip, unlike
 *      mpfr_nexttozero), mant = 2^(prec - 1), exp = EMIN_DEFAULT.
 *      (Ref: mpfr/src/next.c L92-L93 -- `mpfr_setmin(x, __gmpfr_emin)`
 *      without a preceding MPFR_CHANGE_SIGN.)
 *
 *   4. normal: `mant += 1n`. If mant overflows past `2^prec - 1` (i.e.
 *      becomes exactly `2^prec`, the only possible carry-out from
 *      incrementing a prec-bit value):
 *        - if exp == EMAX_DEFAULT: promote to signed Inf (same sign).
 *        - else: exp += 1, mant = 2^(prec - 1) (just the MSB set).
 *      Otherwise the increment is the result.
 *
 * Why "mant >= 2^prec" not "mant > 2^prec - 1"
 * --------------------------------------------
 *
 * They are mathematically equivalent for integer mant; the inequality
 * form against `2^prec` is the one that directly mirrors the C's
 * "carry-out of mpn_add_1" since the only carry-out from incrementing
 * any value in `[2^(prec - 1), 2^prec)` is when mant goes from
 * `2^prec - 1` to `2^prec`. After the carry, the schema-normal value is
 * one binade up.
 *
 * Why returning `x` for Inf is safe
 * ---------------------------------
 *
 * MPFR values are `readonly`-typed and `Object.freeze`-able at the
 * call site. The public ports may return the input value verbatim when
 * the operation is a structural no-op; downstream callers cannot
 * observably distinguish "same reference" from "fresh copy with same
 * fields" because no field is mutable. Returning x for the Inf case
 * avoids a needless `{...x}` allocation.
 *
 * emin / emax
 * -----------
 *
 * We hard-code `EMIN_DEFAULT = -(2^30 - 1)` and `EMAX_DEFAULT = 2^30 - 1`
 * (Ref: mpfr/src/mpfr.h L231-L232). These match `set_exp.ts` and
 * `nexttozero.ts`. The MPFR library exposes thread-local emin/emax via
 * mpfr_set_emin/emax; the TS port does not implement those mutators and
 * the golden driver does not exercise them.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/next.c L87-L117 -- C reference body.
 *   - mpfr/src/setmin.c L26-L37 -- mantissa = 2^(prec-1), exp = emin.
 *   - mpfr/src/mpfr.h L231-L232 -- MPFR_EMAX_DEFAULT, MPFR_EMIN_DEFAULT.
 *   - src/core.ts -- locked MPFR shape, MPFRError.
 *   - src/ops/set_exp.ts -- sister port using the same EMIN/EMAX defaults.
 *   - src/ops/nexttozero.ts -- mirror op (step toward zero).
 *   - CLAUDE.md 'Hallucination-risk callouts' -- NaN propagation, signed
 *     zero observable.
 */

import type { MPFR } from '../core.ts';
import { MPFRError, NAN_VALUE } from '../core.ts';

/**
 * Default exponent bounds, matching MPFR_EMAX_DEFAULT and MPFR_EMIN_DEFAULT.
 *
 * Ref: mpfr/src/mpfr.h L231-L232 -- `#define MPFR_EMAX_DEFAULT
 *   ((mpfr_exp_t) (((mpfr_uexp_t) 1 << 30) - 1))` and
 *   `MPFR_EMIN_DEFAULT = -(EMAX_DEFAULT)`.
 */
const EMAX_DEFAULT: bigint = (1n << 30n) - 1n; // 1073741823n
const EMIN_DEFAULT: bigint = -EMAX_DEFAULT; // -1073741823n

/**
 * Return the immediate successor of `x` in the direction of +/-infinity
 * (preserving x's sign), at x's own precision.
 *
 * @mpfrName mpfr_nexttoinf
 *
 * @param x  The source {@link MPFR}. Any kind.
 *
 * @returns  A fresh `MPFR`. Specifically:
 *           - NaN -> NAN_VALUE.
 *           - +/-Inf -> x unchanged (already at the destination).
 *           - +/-0   -> smallest representable at emin (SAME sign).
 *           - normal -> one ULP further from zero at the same precision.
 *
 * @example
 *   const x = setD(2.0, 53n, 'RNDN').value;       // exp=2, mant=2^52
 *   mpfr_nexttoinf(x);
 *     // -> mant=2^52 + 1, exp=2: the smallest value > 2.0 at prec=53.
 */
export function mpfr_nexttoinf(x: MPFR): MPFR {
  // (1) NaN: propagate. Mirrors the C fall-through.
  // Ref: mpfr/src/next.c L90-L93 -- the singular branch's inner `if`
  // only acts on zero; NaN escapes the body untouched.
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }

  // (2) +/-Inf: no-op. The C does nothing on Inf in the singular branch.
  // Return x verbatim -- the MPFR shape is immutable so this is safe and
  // saves an allocation. Re-validation by the caller still passes.
  if (x.kind === 'inf') {
    return x;
  }

  // (3) +/-0 -> mpfr_setmin(x, emin): smallest representable at emin,
  // sign PRESERVED. (Unlike mpfr_nexttozero, no sign flip.)
  // Ref: mpfr/src/next.c L92-L93 ; mpfr/src/setmin.c L26-L37.
  if (x.kind === 'zero') {
    return {
      kind: 'normal',
      sign: x.sign,
      prec: x.prec,
      exp: EMIN_DEFAULT,
      mant: 1n << (x.prec - 1n),
    };
  }

  // (4) Normal branch.
  if (x.kind !== 'normal') {
    // Exhaustiveness guard. A future schema bump that adds a kind without
    // updating this function should fail loud.
    throw new MPFRError(
      'EPREC',
      `mpfr_nexttoinf: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  // Increment one ULP at the precision boundary. In the TS schema mant
  // is exactly prec bits MSB-aligned, so the C's "MPFR_LIMB_ONE << sh"
  // is just 1n at our level of abstraction.
  // Ref: mpfr/src/next.c L100-L104.
  const newMant = x.mant + 1n;
  const overflowThreshold = 1n << x.prec;

  if (newMant < overflowThreshold) {
    // No carry-out; the simple increment is the result. Most common path.
    return {
      kind: 'normal',
      sign: x.sign,
      prec: x.prec,
      exp: x.exp,
      mant: newMant,
    };
  }

  // Carry-out: mant became exactly 2^prec, equivalent to the binade
  // value 1.000...E[exp+1]. Either promote to Inf (if at emax) or bump
  // the exponent and reset mantissa to the MSB-only normal form.
  // Ref: mpfr/src/next.c L104-L114.
  if (x.exp === EMAX_DEFAULT) {
    // Overflow to +/-Inf, sign preserved.
    // Ref: mpfr/src/next.c L108-L109 -- MPFR_SET_INF(x); sign is not
    // touched, so it carries through from x.
    return {
      kind: 'inf',
      sign: x.sign,
      prec: x.prec,
      exp: 0n,
      mant: 0n,
    };
  }

  // Otherwise: exp += 1, mant = 2^(prec - 1) (MSB-only normal form for
  // the new binade).
  // Ref: mpfr/src/next.c L110-L114 -- MPFR_SET_EXP(x, exp+1);
  //   xp[xn-1] = MPFR_LIMB_HIGHBIT; (lower limbs are already zero from
  //   the carry chain).
  return {
    kind: 'normal',
    sign: x.sign,
    prec: x.prec,
    exp: x.exp + 1n,
    mant: 1n << (x.prec - 1n),
  };
}
