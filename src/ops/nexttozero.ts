/**
 * ops/nexttozero.ts -- pure-TS port of MPFR's `mpfr_nexttozero`.
 *
 * Step `x` one ULP in the direction of zero, at x's own precision. There
 * is no rounding (the "next" value is unique in the precision-defined
 * lattice), so the TS surface returns a fresh {@link MPFR} rather than
 * the canonical `Result {value, ternary}` shape used by rounding ops.
 *
 * C signature
 * -----------
 *
 *   void mpfr_nexttozero(mpfr_ptr x);
 *
 * Body (mpfr/src/next.c L45-L85):
 *
 *   void mpfr_nexttozero (mpfr_ptr x) {
 *     if (MPFR_UNLIKELY (MPFR_IS_SINGULAR (x))) {
 *       if (MPFR_IS_INF (x))
 *         mpfr_setmax (x, __gmpfr_emax);
 *       else {
 *         MPFR_ASSERTN (MPFR_IS_ZERO (x));
 *         MPFR_CHANGE_SIGN(x);
 *         mpfr_setmin (x, __gmpfr_emin);
 *       }
 *     } else {
 *       mp_size_t xn;
 *       int sh;
 *       mp_limb_t *xp;
 *       xn = MPFR_LIMB_SIZE (x);
 *       MPFR_UNSIGNED_MINUS_MODULO (sh, MPFR_PREC(x));
 *       xp = MPFR_MANT(x);
 *       mpn_sub_1 (xp, xp, xn, MPFR_LIMB_ONE << sh);
 *       if (MPFR_UNLIKELY (MPFR_LIMB_MSB (xp[xn-1]) == 0)) {
 *         mpfr_exp_t exp = MPFR_EXP (x);
 *         if (MPFR_UNLIKELY (exp == __gmpfr_emin))
 *           MPFR_SET_ZERO(x);
 *         else {
 *           MPFR_SET_EXP (x, exp - 1);
 *           xp[xn-1] |= MPFR_LIMB_HIGHBIT;
 *         }
 *       }
 *     }
 *   }
 *
 * TS signature
 * ------------
 *
 *   mpfr_nexttozero(x: MPFR): MPFR;
 *
 * Algorithm in the TS schema
 * --------------------------
 *
 * The TS mantissa is already MSB-aligned to exactly `prec` bits (no
 * limb-boundary padding), so the C's "subtract MPFR_LIMB_ONE << sh"
 * collapses to a clean "mant -= 1n" at the TS schema level. Likewise the
 * "MSB no longer set" check becomes `mant < 2^(prec - 1)`.
 *
 *   1. NaN: propagate. The C asserts on NaN (it's a private callee of
 *      mpfr_nextabove/mpfr_nextbelow/mpfr_nexttoward, all of which
 *      NaN-check upstream); the TS port is public, so we propagate
 *      defensively. See "Divergence" in eval/functions/mpfr_nexttozero/spec.json.
 *
 *   2. +/-Inf: emit `mpfr_setmax` analogue at emax: same sign, mant =
 *      2^prec - 1 (every prec-bit set), exp = EMAX_DEFAULT. This is the
 *      largest finite value of the given precision in the default
 *      exponent range.
 *
 *   3. +/-0: SIGN FLIPS then emit `mpfr_setmin` at emin: opposite sign,
 *      mant = 2^(prec - 1) (just the MSB), exp = EMIN_DEFAULT. The sign
 *      flip is load-bearing -- mpfr_nextbelow on +0 routes here and
 *      expects to land on -minSubnormal per IEEE 754 nextDown semantics.
 *      (Ref: mpfr/src/next.c L57 -- `MPFR_CHANGE_SIGN(x)` precedes
 *      `mpfr_setmin`.)
 *
 *   4. normal: `mant -= 1n`. If the MSB drops (only possible when the
 *      pre-decrement mant was exactly `2^(prec - 1)`, an exact power of
 *      two; equivalently the post-decrement mant equals `2^(prec - 1) - 1`):
 *        - if exp == EMIN_DEFAULT: collapse to signed zero (same sign).
 *        - else: exp -= 1, mant = 2^prec - 1 (all prec bits set).
 *      Otherwise the decrement is the result.
 *
 * Why the "MSB dropped" check is `mant < 2^(prec - 1)`
 * ----------------------------------------------------
 *
 * On entry mant is in `[2^(prec - 1), 2^prec)` (MPFR's MSB-normalisation
 * invariant). After `mant -= 1n`, mant lies in `[2^(prec - 1) - 1, 2^prec - 1)`.
 * The MSB at position `prec - 1` is unset iff mant == 2^(prec - 1) - 1,
 * iff `mant < 2^(prec - 1)`. The bit-mask `mant & 2^(prec-1)` would work
 * equally well; the inequality form is clearer.
 *
 * emin / emax
 * -----------
 *
 * We hard-code `EMIN_DEFAULT = -(2^30 - 1)` and `EMAX_DEFAULT = 2^30 - 1`
 * (Ref: mpfr/src/mpfr.h L231-L232). These match `set_exp.ts`. The MPFR
 * library exposes thread-local emin/emax via mpfr_set_emin/emax; the TS
 * port does not implement those mutators and the golden driver does not
 * exercise them. Should a future ADR add emin/emax mutation, this
 * constant moves to a shared module.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/next.c L45-L85 -- C reference body.
 *   - mpfr/src/setmin.c L26-L37 -- mantissa = 2^(prec-1), exp = emin.
 *   - mpfr/src/setmax.c L26-L40 -- mantissa = 2^prec - 1, exp = emax.
 *   - mpfr/src/mpfr.h L231-L232 -- MPFR_EMAX_DEFAULT, MPFR_EMIN_DEFAULT.
 *   - src/core.ts -- locked MPFR shape, MPFRError.
 *   - src/ops/set_exp.ts -- sister port using the same EMIN/EMAX defaults.
 *   - src/ops/nexttoinf.ts -- mirror op (step toward +/-infinity).
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
 * Return the immediate predecessor of `x` in the direction of zero, at
 * x's own precision.
 *
 * @mpfrName mpfr_nexttozero
 *
 * @param x  The source {@link MPFR}. Any kind.
 *
 * @returns  A fresh `MPFR`. Specifically:
 *           - NaN -> NAN_VALUE.
 *           - +/-Inf -> largest finite at emax (same sign).
 *           - +/-0   -> smallest representable at emin (OPPOSITE sign).
 *           - normal -> one ULP closer to zero at the same precision.
 *
 * @example
 *   const x = setD(2.0, 53n, 'RNDN').value;       // exp=2, mant=2^52
 *   mpfr_nexttozero(x);
 *     // -> mant=2^53-1, exp=1: the largest value < 2.0 at prec=53.
 */
export function mpfr_nexttozero(x: MPFR): MPFR {
  // (1) NaN: defensively propagate. The C asserts; we don't.
  // Ref: mpfr/src/next.c L56 (MPFR_ASSERTN in the singular-but-not-Inf
  // branch only succeeds on zero).
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }

  // (2) +/-Inf -> mpfr_setmax(x, emax): largest finite at emax, sign
  // preserved.
  // Ref: mpfr/src/next.c L50-L53 ; mpfr/src/setmax.c L26-L40.
  if (x.kind === 'inf') {
    return {
      kind: 'normal',
      sign: x.sign,
      prec: x.prec,
      exp: EMAX_DEFAULT,
      mant: (1n << x.prec) - 1n,
    };
  }

  // (3) +/-0 -> sign flip, then smallest representable at emin.
  // Ref: mpfr/src/next.c L56-L59 -- MPFR_CHANGE_SIGN(x); mpfr_setmin(x, emin).
  if (x.kind === 'zero') {
    const newSign: 1 | -1 = x.sign === 1 ? -1 : 1;
    return {
      kind: 'normal',
      sign: newSign,
      prec: x.prec,
      exp: EMIN_DEFAULT,
      mant: 1n << (x.prec - 1n),
    };
  }

  // (4) Normal branch.
  if (x.kind !== 'normal') {
    // Exhaustiveness guard. The four kinds are nan/inf/zero/normal; the
    // above branches cover the first three. A future schema bump that
    // adds a kind without updating this function should fail loud.
    throw new MPFRError(
      'EPREC',
      `mpfr_nexttozero: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  // Decrement one ULP at the precision boundary. In the TS schema mant
  // is exactly prec bits MSB-aligned, so the C's "MPFR_LIMB_ONE << sh"
  // is just 1n at our level of abstraction.
  // Ref: mpfr/src/next.c L67-L70.
  const newMant = x.mant - 1n;
  const msbMask = 1n << (x.prec - 1n);

  if (newMant >= msbMask) {
    // MSB still set; the simple decrement is the result. Most common path.
    return {
      kind: 'normal',
      sign: x.sign,
      prec: x.prec,
      exp: x.exp,
      mant: newMant,
    };
  }

  // MSB dropped -- x was an exact power of two (mant == 2^(prec-1)
  // pre-decrement). Renormalize: either underflow to zero or shift
  // the exponent down and fill all prec bits.
  // Ref: mpfr/src/next.c L71-L82.
  if (x.exp === EMIN_DEFAULT) {
    // Underflow to signed zero, sign preserved.
    // Ref: mpfr/src/next.c L75-L76 -- MPFR_SET_ZERO(x); sign is not
    // touched, so it carries through from x.
    return {
      kind: 'zero',
      sign: x.sign,
      prec: x.prec,
      exp: 0n,
      mant: 0n,
    };
  }

  // Otherwise: exp -= 1, mant = 2^prec - 1 (all prec bits set).
  // The C achieves this by xp[xn-1] |= HIGHBIT after the subtract; the
  // lower bits of the limb are already 1s from the borrow chain on
  // 0x10...0 - 1 = 0x0F...F (at the prec-aligned level).
  // Ref: mpfr/src/next.c L77-L82.
  return {
    kind: 'normal',
    sign: x.sign,
    prec: x.prec,
    exp: x.exp - 1n,
    mant: (1n << x.prec) - 1n,
  };
}
