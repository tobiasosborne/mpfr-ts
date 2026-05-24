/**
 * ops/neg.ts — pure-TS port of MPFR's `mpfr_neg`.
 *
 * Negate an {@link MPFR} value at the caller-supplied target precision,
 * rounded per the rounding mode, returning the canonical
 * `{value, ternary}` shape from src/core.ts.
 *
 * C signature
 * -----------
 *
 *   int mpfr_neg(mpfr_t rop, mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   - mutates `rop` in place (precision comes from `rop`);
 *   - sets `rop` to `-op` (rounded if `prec(rop) != prec(op)`);
 *   - returns the ternary as the function result.
 *
 *   The C dispatch in mpfr/src/neg.c L24–L37 splits on the a==b alias:
 *
 *     if (a != b)
 *       return mpfr_set4(a, b, rnd_mode, -MPFR_SIGN(b));
 *     else {
 *       MPFR_CHANGE_SIGN(a);
 *       if (MPFR_IS_NAN(b)) MPFR_RET_NAN;
 *       else MPFR_RET(0);
 *     }
 *
 *   — i.e. the general path is "copy b's mantissa into a (rounding to
 *   a's prec) and impose sign = -SIGN(b)"; the in-place path just flips
 *   the sign bit (always exact, ternary 0). The underlying `mpfr_set4`
 *   is the load-bearing primitive — see mpfr/src/set.c L25–L64.
 *
 * TS signature
 * ------------
 *
 *   mpfr_neg(x: MPFR, prec: bigint, rnd: RoundingMode): Result;
 *
 *   - takes `prec` as an explicit positional argument (no `rop` alias);
 *   - returns the immutable {@link Result} from src/core.ts (Law 4).
 *
 * Algorithm
 * ---------
 *
 * Top-level dispatch on `x.kind`:
 *
 *   1. NaN: return `{value: NAN_VALUE, ternary: 0}`. The C reference
 *      returns NaN with ERANGE flag set; our schema collapses every NaN
 *      to the single `NAN_VALUE` sentinel (sign field is meaningless;
 *      `Sign === 1` by convention).
 *
 *   2. ±Inf: flip the sign, preserve kind. Exact, ternary 0.
 *
 *   3. ±0: flip the sign, preserve kind. Exact, ternary 0. Signed zero
 *      is observable in MPFR (`+0` vs `-0` differ under RNDD addition),
 *      so the flip must be propagated through the locked schema.
 *
 *   4. normal: result sign = `-x.sign`, but the mantissa needs the
 *      same prec-conversion treatment as `mpfr_set`:
 *
 *        - If `prec >= x.prec`: lossless pad. New mantissa is
 *          `x.mant << (prec - x.prec)`, exponent unchanged, ternary 0.
 *
 *        - If `prec <  x.prec`: round `x.mant` from `x.prec` bits down
 *          to `prec` bits via the shared `roundMantissa` primitive.
 *          **The rounding direction depends on the NEW sign**, not on
 *          x's old sign. mpfr/src/neg.c L28 wires this via
 *          `mpfr_set4(..., -MPFR_SIGN(b))`, and `mpfr_set4`'s rounding
 *          step is parameterised by the passed-in `signb`
 *          (mpfr/src/set.c L58: `MPFR_RNDRAW(inex, a, MPFR_MANT(b),
 *          MPFR_PREC(b), rnd_mode, signb, ...)`). The TS substrate
 *          `roundMantissa` takes the sign explicitly for the same
 *          reason; we pass the negated sign.
 *
 * Why the rounding-sign distinction matters
 * -----------------------------------------
 *
 * Consider `mpfr_neg(x, prec, 'RNDU')` where `x` is a positive normal
 * whose mantissa rounds inexactly into a lower target precision:
 *
 *   - x = +1.1011 (5 bits)
 *   - target prec = 3 bits, rnd = RNDU.
 *
 * The "exact" intermediate `-x` is `-1.1011`. Under RNDU (round toward
 * +∞), the rounded magnitude should DECREASE (round toward zero from the
 * negative side, toward +∞ globally). If we naively passed `x.sign = +1`
 * to `roundMantissa`, it would treat `+1.1011` and round under RNDU as
 * "increment" → `+1.110` → ternary +1. Inverting the sign at the end
 * gives `-1.110`, ternary -1, but the value `-1.110` is LESS than the
 * exact `-1.1011`, not greater — wrong ternary direction.
 *
 * Passing `-x.sign = -1` to `roundMantissa` correctly routes the RNDU
 * branch as "increment iff sign = +1 → no increment" → truncate to
 * `1.101`, attach the negative sign → `-1.101`, ternary +1 (rounded
 * greater than exact, i.e. less negative — correct for RNDU on a
 * negative). This is the subtlety the C `mpfr_set4(..., -SIGN(b))` call
 * encodes; getting it wrong silently inverts ternary on the
 * `prec < x.prec` path while leaving the value correct.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/neg.c L24–L37 — the C reference (delegates to
 *     `mpfr_set4` with a flipped sign).
 *   - mpfr/src/set.c L25–L64 — `mpfr_set4`, the load-bearing primitive.
 *     The `MPFR_PREC(b) == MPFR_PREC(a)` branch (L45–L52) just copies
 *     the mantissa (ternary 0); the else branch (L54–L63) calls
 *     MPFR_RNDRAW with the passed-in `signb`.
 *   - src/internal/mpfr/round_raw.ts — the shared substrate primitive.
 *   - src/core.ts — locked `MPFR` / `RoundingMode` / `Result` / `Sign`
 *     types; `posInf` / `negInf` / `posZero` / `negZero` / `NAN_VALUE`.
 *   - CLAUDE.md "Hallucination-risk callouts" — Signed zero observable
 *     (sign flip must propagate); Ternary direction is sign of
 *     (rounded - exact) AT THE RESULT SIGN.
 */

import type { MPFR, Result, RoundingMode, Sign } from '../core.ts';
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
 * Validate the public-boundary scalar arguments. Throws `MPFRError` on
 * bad inputs. Per the convention established in add.ts / mul.ts:
 * we trust the input MPFR's shape (the runner pre-validates via
 * decodeMpfr) and only re-check the `prec` / `rnd` arguments here.
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
 * Negate an MPFR value at the target precision.
 *
 * @mpfrName mpfr_neg
 *
 * @param x     the operand. Any kind (`'normal'`, `'zero'`, `'inf'`, `'nan'`).
 * @param prec  output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 * @param rnd   one of the five {@link RoundingMode} values.
 *
 * @returns     `{value, ternary}`. The value passes `validate()` without
 *              post-processing. Ternary is `0` for exact (every special,
 *              and every `prec >= x.prec` normal), `±1` only when the
 *              `prec < x.prec` rounding step lost bits.
 *
 * @throws {MPFRError} `EPREC` on bad precision; `EROUND` on bad rounding
 *                    mode. NaN / Inf input is NOT an error.
 *
 * @example
 *   neg(setD(3.14, 53n, 'RNDN').value, 53n, 'RNDN');
 *     // → {value: -3.14 at prec 53, ternary: 0}  (lossless same-prec)
 *   neg(posZero(53n), 53n, 'RNDN');
 *     // → {value: negZero(53n), ternary: 0}
 *   neg(posInf(53n), 53n, 'RNDN');
 *     // → {value: negInf(53n), ternary: 0}
 *   neg(NAN_VALUE, 53n, 'RNDN');
 *     // → {value: NAN_VALUE, ternary: 0}
 */
export function mpfr_neg(x: MPFR, prec: bigint, rnd: RoundingMode): Result {
  validateArgs(prec, rnd);

  // --- Specials --------------------------------------------------------
  // (1) NaN propagation. The C reference returns NaN + sets ERANGE; our
  // canonical schema folds every NaN to NAN_VALUE.
  if (x.kind === 'nan') {
    return { value: NAN_VALUE, ternary: 0 };
  }

  // (2) ±Inf: flip sign, preserve kind. Exact.
  if (x.kind === 'inf') {
    // x.sign is 1 or -1; flipping gives the other one. We branch
    // explicitly on the post-flip sign to call the right factory
    // (the factories enforce prec validation symmetrically).
    return {
      value: x.sign === 1 ? negInf(prec) : posInf(prec),
      ternary: 0,
    };
  }

  // (3) ±0: flip sign, preserve kind. Signed zero is observable
  // (CLAUDE.md "Signed zero is real"); the flip must be propagated.
  if (x.kind === 'zero') {
    return {
      value: x.sign === 1 ? negZero(prec) : posZero(prec),
      ternary: 0,
    };
  }

  // (4) Normal: flip the sign, then either lossless-pad (prec >= x.prec)
  // or round (prec < x.prec) the mantissa. The result sign is the
  // negated x.sign; the rounding-mode interpretation must use the
  // NEW sign (see the "Why the rounding-sign distinction matters"
  // section of the module docstring above).
  if (x.kind !== 'normal') {
    // Defensive: unreachable (the four kinds are dispatched above).
    // Surface as a precise error rather than fall through with garbage.
    throw new MPFRError(
      'EPREC',
      `mpfr_neg: unexpected kind ${String((x as { kind?: unknown }).kind)}`,
    );
  }

  const newSign: Sign = (-x.sign) as Sign;

  if (prec >= x.prec) {
    // Lossless: left-pad with zeros to widen the mantissa frame from
    // x.prec to prec bits. The mantissa value (and hence exp) is
    // unchanged in absolute terms; only the MSB-alignment shifts.
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

  // Lossy: round x.mant from x.prec bits down to prec bits. Pass the
  // NEW sign so RNDU/RNDD route correctly through roundMantissa.
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
