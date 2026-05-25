/**
 * ops/custom_init_set.ts -- pure-TS port of MPFR's `mpfr_custom_init_set`.
 *
 * Constructs an MPFR value from explicit `(kind, exp, prec, mantissa)`
 * fields. The C body decodes a signed `kind` argument:
 *
 *     if (kind >= 0) { type = kind;  sign = +1; }
 *     else           { type = -kind; sign = -1; }
 *
 * with `|kind|` ranging over MPFR's `mpfr_kind_t` enum -- `0` for NaN,
 * `1` for Inf, `2` for Zero, `3` for Regular -- and the exponent field
 * set either to `exp` (Regular) or to the matching sentinel.
 *
 * The TS port mirrors this decode and then dispatches to the canonical
 * schema constructors (`NAN_VALUE`, `posInf`/`negInf`, `posZero`/
 * `negZero`, or a fresh `normal` MPFR validated by `validate()`). The
 * `mantissa` parameter is the MSB-aligned mantissa value as a bigint
 * (no caller-managed buffer in the immutable surface).
 *
 * Divergences worth flagging:
 *
 *   - NaN kind always returns the canonical `NAN_VALUE` (`prec=0n`,
 *     `sign=1`), so the caller's `prec` is discarded for NaN. This
 *     matches the locked schema's NaN representation.
 *   - The C function only debug-asserts on malformed inputs
 *     (`MPFR_ASSERTD`); this port surfaces malformed Regular tuples
 *     through `validate()` -> `EPREC`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/stack_interface.c L60-L89 -- C reference body.
 *   - mpfr/src/mpfr.h L287-L292 -- `mpfr_kind_t` enum (NAN=0, INF=1,
 *     ZERO=2, REGULAR=3).
 *   - src/core.ts L243-L249 -- canonical NAN_VALUE.
 *   - src/core.ts L113-L135 -- MPFR shape invariants enforced by validate.
 */

import type { MPFR } from '../core.ts';
import {
  MPFRError,
  NAN_VALUE,
  posInf,
  negInf,
  posZero,
  negZero,
  validate,
} from '../core.ts';

/**
 * Construct an MPFR value from explicit kind/exp/prec/mantissa fields.
 *
 * @mpfrName mpfr_custom_init_set
 *
 * @param kind      Signed encoding: `0` (NaN), `±1` (Inf), `±2` (Zero),
 *                  `±3` (Regular). Sign of `kind` carries the value sign;
 *                  `|kind|` selects the discriminant.
 * @param exp       Base-2 exponent (used only for Regular kind).
 * @param prec      Precision in bits (used only for non-NaN kinds).
 * @param mantissa  MSB-aligned mantissa as a bigint (used only for Regular).
 * @returns         A validated MPFR value of the requested shape.
 *
 * @throws {MPFRError} `EDOMAIN` for malformed `kind`/argument types;
 *                     `EPREC` for malformed Regular tuples (e.g. mantissa
 *                     not MSB-aligned).
 *
 * @example
 *   mpfr_custom_init_set(0, 0n, 53n, 0n);             // NaN
 *   mpfr_custom_init_set(1, 0n, 53n, 0n);             // +Inf @ 53 bits
 *   mpfr_custom_init_set(-2, 0n, 53n, 0n);            // -0  @ 53 bits
 *   mpfr_custom_init_set(3, 1n, 1n, 1n);              // +1.0 @ 1 bit
 */
export function mpfr_custom_init_set(
  kind: number,
  exp: bigint,
  prec: bigint,
  mantissa: bigint,
): MPFR {
  if (typeof kind !== 'number' || !Number.isInteger(kind)) {
    throw new MPFRError('EDOMAIN', `mpfr_custom_init_set: kind must be an integer`);
  }
  if (typeof exp !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_custom_init_set: exp must be bigint`);
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_custom_init_set: prec must be bigint`);
  }
  if (typeof mantissa !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_custom_init_set: mantissa must be bigint`);
  }

  // Decode (sign, type) per stack_interface.c L70-L78.
  const sign: 1 | -1 = kind < 0 ? -1 : 1;
  const type = kind < 0 ? -kind : kind;

  switch (type) {
    case 0:
      // NAN_KIND: always the canonical schema NaN. `prec` is discarded
      // because NaN has no meaningful precision in this surface.
      return NAN_VALUE;
    case 1:
      return sign === 1 ? posInf(prec) : negInf(prec);
    case 2:
      return sign === 1 ? posZero(prec) : negZero(prec);
    case 3: {
      // REGULAR_KIND: construct and validate. validate() catches
      // malformed mantissas (not MSB-aligned, out of range) and bad
      // precision -- the equivalent of the C MPFR_ASSERTD checks,
      // but enforced at runtime (Law 1: fail loud).
      const v: MPFR = {
        kind: 'normal',
        sign,
        prec,
        exp,
        mant: mantissa,
      };
      validate(v);
      return v;
    }
    default:
      throw new MPFRError(
        'EDOMAIN',
        `mpfr_custom_init_set: invalid kind value ${kind} (|kind| must be 0..3)`,
      );
  }
}
