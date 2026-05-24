/**
 * ops/get_ui.ts — pure-TS port of MPFR's `mpfr_get_ui`.
 *
 * Convert an {@link MPFR} value to an unsigned 64-bit integer, rounded
 * per the rounding mode. Inverse-shaped to `mpfr_set_ui`.
 *
 * C signature
 * -----------
 *
 *   unsigned long mpfr_get_ui(mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Behaviour (mpfr/src/get_ui.c L25–L83):
 *     - NaN              → 0, sets ERANGE flag
 *     - +Inf / overflows → ULONG_MAX, sets ERANGE flag
 *     - -Inf / -value not rounding to 0 → 0, sets ERANGE flag
 *     - ±0               → 0 (exact)
 *     - finite in range  → rounded integer per rnd
 *
 *   The C `mpfr_fits_ulong_p` (mpfr/src/fits_u.h L24–L78) treats a
 *   negative value as fitting iff its rounded form is `0` — e.g. `-0.4`
 *   under RNDN fits (rounds to 0); `-1.5` does not. We mirror that
 *   contract, but throw instead of saturating.
 *
 * TS divergence
 * -------------
 *
 *   mpfr_get_ui(x: MPFR, rnd: RoundingMode): bigint;   // throws on ERANGE
 *
 * Same rationale as get_si.ts: the locked schema has no ERANGE flag,
 * and silent saturation hides bugs. Discriminant is `EPREC` (same
 * convention as get_si and set_si/set_ui's range errors).
 *
 * Algorithm
 * ---------
 *
 *   1. Validate `rnd`; structurally validate `x`.
 *   2. Specials:
 *        NaN  → throw
 *        ±Inf → throw
 *        ±0   → return 0n
 *   3. Normal: round the absolute value to an integer per `rnd` (using
 *      the value's sign so RNDU/RNDD route correctly). Then:
 *        - sign === +1: if rounded > ULONG_MAX, throw; otherwise return.
 *        - sign === -1: if rounded === 0n (the rounding decided to
 *          truncate toward 0), return 0n. Otherwise the value did not
 *          fit per `mpfr_fits_ulong_p` — throw.
 *
 *      The "negative rounding to 0" case maps directly to fits_u.h's
 *      L39–L43 branch: the value fits unsigned iff its rnd-mode round
 *      is exactly 0.
 *
 * The integer-rounding helper (`roundToInteger`) is duplicated from
 * `get_si.ts` rather than extracted. Two callers does not yet pass the
 * "third caller forces a substrate" threshold the project applied to
 * `roundMantissa`; an internal/mpfr/round_to_integer.ts extraction will
 * happen when a third get-family op (mpfr_get_z, mpfr_get_uj) lands.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/get_ui.c L25–L83 — C reference.
 *   - mpfr/src/fits_u.h — the underlying mpfr_fits_ulong_p logic,
 *     specifically the negative-value branch L39–L43.
 *   - src/internal/mpfr/round_raw.ts — shared rounding substrate.
 *   - src/ops/get_si.ts — signed sibling; same algorithm structure.
 *   - src/core.ts — locked schema.
 *   - CLAUDE.md "Hallucination-risk callouts" — same set as get_si.
 */

import type { MPFR, RoundingMode, Sign } from '../core.ts';
import { MPFRError, validate } from '../core.ts';
import { roundMantissa } from '../internal/mpfr/round_raw.ts';

const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

/**
 * Validate the rounding mode argument.
 */
function validateRnd(rnd: RoundingMode): void {
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
 * Round a non-singular MPFR to an integer per `rnd`, returning the
 * absolute integer magnitude. Mirrors `roundToInteger` in get_si.ts —
 * see that file for full commentary on the three sub-cases.
 *
 * Pre-condition: `x.kind === 'normal'` and `x` passes `validate()`.
 */
function roundToInteger(x: MPFR, sign: Sign, rnd: RoundingMode): bigint {
  const exp = x.exp;
  const prec = x.prec;
  const mant = x.mant;

  if (exp >= prec) {
    return mant << (exp - prec);
  }

  if (exp <= 0n) {
    switch (rnd) {
      case 'RNDZ':
        return 0n;
      case 'RNDA':
        return 1n;
      case 'RNDD':
        return sign === -1 ? 1n : 0n;
      case 'RNDU':
        return sign === 1 ? 1n : 0n;
      case 'RNDN': {
        if (exp < 0n) {
          return 0n;
        }
        const halfMant = 1n << (prec - 1n);
        if (mant > halfMant) {
          return 1n;
        }
        return 0n; // tie at 1/2 — even is 0
      }
      default: {
        const _exhaustive: never = rnd;
        void _exhaustive;
        throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
      }
    }
  }

  const rounded = roundMantissa(mant, prec, exp, exp, sign, rnd);
  if (rounded.exp === exp) {
    return rounded.mant;
  }
  // Carry-out: integer magnitude is 2^exp.
  return 1n << exp;
}

/**
 * Convert an MPFR value to an unsigned 64-bit integer, rounded per `rnd`.
 *
 * @mpfrName mpfr_get_ui
 *
 * @param x    the MPFR value. Must pass {@link validate}.
 * @param rnd  one of the five rounding modes.
 *
 * @returns    the rounded integer as a `bigint` in `[0, ULONG_MAX]`.
 *
 * @throws {MPFRError}
 *   - `EROUND` on unknown rounding mode.
 *   - `EPREC` if `x` is NaN, ±Inf, a negative value that doesn't round
 *     to 0, or rounds to a positive value exceeding ULONG_MAX.
 *
 * @example
 *   mpfr_get_ui(setUi(42n, 53n, 'RNDN').value, 'RNDN');     // 42n
 *   mpfr_get_ui(setD(3.7, 53n, 'RNDN').value, 'RNDZ');      // 3n
 *   mpfr_get_ui(setD(-0.4, 53n, 'RNDN').value, 'RNDN');     // 0n  (rounds to 0)
 *   // mpfr_get_ui(setD(-1.5, 53n, 'RNDN').value, 'RNDN');  // throws (rounds to -2, doesn't fit)
 */
export function mpfr_get_ui(x: MPFR, rnd: RoundingMode): bigint {
  validateRnd(rnd);
  validate(x);

  // --- Specials ------------------------------------------------------------
  if (x.kind === 'nan') {
    throw new MPFRError(
      'EPREC',
      'mpfr_get_ui: NaN cannot be converted to an unsigned integer',
    );
  }
  if (x.kind === 'inf') {
    throw new MPFRError(
      'EPREC',
      `mpfr_get_ui: ${x.sign === 1 ? '+Inf' : '-Inf'} cannot be converted to an unsigned integer`,
    );
  }
  if (x.kind === 'zero') {
    return 0n;
  }

  // --- Normal: round and bounds-check --------------------------------------
  const sign = x.sign;
  const absInt = roundToInteger(x, sign, rnd);

  if (sign === 1) {
    if (absInt > ULONG_MAX_VAL) {
      throw new MPFRError(
        'EPREC',
        `mpfr_get_ui: value rounds to ${absInt}, exceeds ULONG_MAX=${ULONG_MAX_VAL}`,
      );
    }
    return absInt;
  }
  // sign === -1: only "rounds-to-0" fits unsigned. Mirrors fits_u.h L39–L43.
  if (absInt === 0n) {
    return 0n;
  }
  throw new MPFRError(
    'EPREC',
    `mpfr_get_ui: negative value rounds to -${absInt}, doesn't fit unsigned`,
  );
}
