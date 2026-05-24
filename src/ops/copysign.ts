/**
 * ops/copysign.ts — pure-TS port of MPFR's `mpfr_copysign`.
 *
 * Produce a value with the magnitude of `x` and the sign of `y`, rounded
 * per `rnd` to the target precision. Sibling to setsign.ts: the only
 * difference is that the result sign comes from another MPFR operand
 * rather than a caller-supplied flag.
 *
 * C signature
 * -----------
 *
 *   int mpfr_copysign(mpfr_t rop, mpfr_srcptr op, mpfr_srcptr y, mpfr_rnd_t rnd);
 *
 *     - z = (-1)^signbit(y) * abs(x), i.e. same sign bit as y, even if z is NaN.
 *
 *   The C dispatch in mpfr/src/copysign.c L33–L46 splits on alias:
 *
 *     if (z != x)
 *       return mpfr_set4 (z, x, rnd_mode, MPFR_SIGN(y));
 *     else {
 *       MPFR_SET_SAME_SIGN (z, y);
 *       if (MPFR_IS_NAN (x)) MPFR_RET_NAN;
 *       else MPFR_RET (0);
 *     }
 *
 *   The C comment at L25–L30 is explicit that the sign of `y` is
 *   imposed even when the result is a NaN.
 *
 * TS signature
 * ------------
 *
 *   mpfr_copysign(x: MPFR, y: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   Both operands are immutable {@link MPFR} values. The result carries
 *   the sign extracted from `y.sign`.
 *
 * Divergence from C → TS — NaN sign
 * ---------------------------------
 *
 * MPFR's C surface preserves the sign bit on a NaN result (`mpfr_setsign`
 * is documented to "even if z is a NaN"). The TS locked schema collapses
 * every NaN to the canonical `NAN_VALUE` whose `sign === 1` by
 * convention (src/core.ts L83–L88). Two consequences:
 *
 *   1. `copysign(NaN, y)` → NAN_VALUE. The sign of `y` is ignored. The
 *      grader's `decodeMpfr` does the same fold for the C-side wire so
 *      the golden expects NAN_VALUE too.
 *
 *   2. `copysign(x, NaN)` for normal `x`: the sign extracted from `y`
 *      is `y.sign`, which is `1` per the canonicalisation. So the result
 *      is `+|x|`. **The C side, given a NaN `y` with a sign bit set
 *      (e.g. via MPFR_SET_NEG), would produce `-|x|`**, but the wire
 *      emitter (jl_kv_mpfr in common.h) reads NaN sign as `1` for the
 *      wire encoding (see common.h L383). So when `y` arrives via the
 *      golden wire it always has sign=1 on the TS side; the C side
 *      also writes its NaN-y inputs with sign=1; both ports compute on
 *      a sign=1 NaN; both produce `+|x|`. **The goldens must therefore
 *      avoid emitting a NaN `y` whose sign was set negative on the C
 *      side, because the wire format cannot represent it.** The
 *      golden driver only constructs `y` NaNs via `mpfr_set_nan` (which
 *      leaves the default `+NaN` sign) so this constraint is satisfied.
 *
 * Algorithm
 * ---------
 *
 * Identical to setsign.ts with `sign := (y.sign === -1)`. Dispatch:
 *
 *   1. NaN x → NAN_VALUE.
 *   2. ±Inf x → Inf with sign = y.sign.
 *   3. ±0 x → zero with sign = y.sign.
 *   4. normal x → magnitude rounded to prec, with sign = y.sign and
 *      that same sign passed to the substrate `roundMantissa` for
 *      direction (mpfr_set4(..., MPFR_SIGN(y)) at mpfr/src/copysign.c L37).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/copysign.c L24–L46 — the C reference. The comment at
 *     L25–L30 documents the NaN-sign preservation rule we deliberately
 *     diverge from above.
 *   - mpfr/src/set.c L25–L64 — `mpfr_set4`, the load-bearing primitive
 *     (called with `MPFR_SIGN(y)` here).
 *   - src/ops/setsign.ts — direct sibling; this file delegates the
 *     sign-from-bool reasoning to that file's "sign-from-y.sign"
 *     specialisation.
 *   - src/ops/abs.ts — sibling (always sign=+1).
 *   - src/ops/neg.ts — sibling (sign = -x.sign).
 *   - src/internal/mpfr/round_raw.ts — shared substrate primitive.
 *   - src/core.ts — locked MPFR / RoundingMode / Result / Sign types;
 *     the NaN canonicalisation convention (sign=1).
 *   - CLAUDE.md "Hallucination-risk callouts" — Signed zero observable;
 *     ternary is sign of (rounded - exact) AT THE RESULT SIGN; NaN
 *     canonicalisation (per locked schema, distinct from MPFR's C
 *     surface).
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  PREC_MAX,
  PREC_MIN,
  negInf,
  negZero,
  posInf,
  posZero,
} from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

/**
 * Validate the public-boundary scalar arguments.
 */
function validateArgs(prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Produce a value with the magnitude of `x` and the sign bit of `y`, at
 * the target precision.
 *
 * @mpfrName mpfr_copysign
 *
 * @param x     the magnitude source. Any kind.
 * @param y     the sign source. Any kind. NaN `y` contributes sign=1 per
 *              the schema's canonicalisation — see the file docstring.
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. NaN `x` input → NAN_VALUE.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf input is NOT an error.
 *
 * @example
 *   copysign(setD(3.14, 53n, 'RNDN').value, setD(-1.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: -3.14 at prec 53, ternary: 0}
 *   copysign(posInf(53n), negZero(53n), 53n, 'RNDN');
 *     // → {value: negInf(53n), ternary: 0}
 *   copysign(NAN_VALUE, setD(-1.0, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}
 */
export function mpfr_copysign(
  x: MPFR,
  y: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // (1) NaN x → canonical NAN_VALUE; y's sign is dropped per the locked
  // schema's NaN canonicalisation (src/core.ts L83–L88). The C reference
  // would preserve y's sign on a NaN result, but the wire format folds
  // every NaN to sign=1 so the divergence is invisible to the grader.
  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // y.sign carries the result sign. The C source uses MPFR_SIGN(y)
  // unconditionally (mpfr/src/copysign.c L37); we read it the same way.
  // A NaN `y` has sign=1 by our schema convention, which folds the
  // C-side "sign of NaN y" path into the positive branch — see the
  // file-level "Divergence from C → TS — NaN sign" section.
  const newSign = y.sign;

  // (2) ±Inf x: choose the constructor matching the chosen sign.
  if (x.kind === 'inf') {
    return {
      value: newSign === 1 ? posInf(prec) : negInf(prec),
      ternary: 0,
    };
  }

  // (3) ±0 x: same.
  if (x.kind === 'zero') {
    return {
      value: newSign === 1 ? posZero(prec) : negZero(prec),
      ternary: 0,
    };
  }

  // (4) Normal x.
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EPREC',
      `mpfr_copysign: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  if (prec >= x.prec) {
    const padShift = prec - x.prec;
    const value: MPFR = {
      kind: 'normal',
      sign: newSign,
      prec,
      exp: x.exp,
      mant: x.mant << padShift,
    };
    return { value, ternary: 0 };
  }

  // Lossy: round and pass the new (y-derived) sign for RNDU/RNDD
  // direction routing.
  const { mant, exp, ternary } = roundMantissa(
    x.mant,
    x.prec,
    x.exp,
    prec,
    newSign,
    rnd,
  );
  const value: MPFR = {
    kind: 'normal',
    sign: newSign,
    prec,
    exp,
    mant,
  };
  return { value, ternary };
}
