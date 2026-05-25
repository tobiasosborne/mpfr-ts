/**
 * reference_ports/correct/mpfr_custom_init_set.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/stack_interface.c L60-L89):
 *   if kind >= 0: type = kind, sign = +1
 *   else:         type = -kind, sign = -1
 *   build MPFR(type, sign, exp, prec, mantissa) and return
 *
 * Kind encoding (mpfr/src/mpfr.h L287-L292):
 *   NAN_KIND = 0, INF_KIND = 1, ZERO_KIND = 2, REGULAR_KIND = 3.
 *
 * Returns NAN_VALUE for NAN kind (matches schema canonicalization).
 *
 * Ref: mpfr/src/stack_interface.c L60-L89 -- C reference.
 * Ref: src/core.ts -- MPFR shape, constructors, validate.
 */

import type { MPFR } from '../../../src/core.ts';
import {
  MPFRError,
  NAN_VALUE,
  posInf,
  negInf,
  posZero,
  negZero,
  validate,
} from '../../../src/core.ts';

export function mpfr_custom_init_set(
  kind: number,
  exp: bigint,
  prec: bigint,
  mantissa: bigint,
): MPFR {
  if (typeof kind !== 'number' || !Number.isInteger(kind)) {
    throw new MPFRError('EDOMAIN', `mpfr_custom_init_set: kind must be int`);
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

  // Decode kind: sign(kind) -> sign, |kind| -> type.
  const sign = kind < 0 ? -1 : 1;
  const type = kind < 0 ? -kind : kind;

  // NAN_KIND = 0 -> always NAN_VALUE (schema canonicalization).
  if (type === 0) {
    return NAN_VALUE;
  }
  if (type === 1) {
    // INF_KIND
    return sign === 1 ? posInf(prec) : negInf(prec);
  }
  if (type === 2) {
    // ZERO_KIND
    return sign === 1 ? posZero(prec) : negZero(prec);
  }
  if (type === 3) {
    // REGULAR_KIND -- construct a normal value and validate.
    const v: MPFR = {
      kind: 'normal',
      sign: sign as 1 | -1,
      prec,
      exp,
      mant: mantissa,
    };
    validate(v);  // throws EPREC on malformed (e.g., mant not MSB-aligned)
    return v;
  }

  throw new MPFRError(
    'EDOMAIN',
    `mpfr_custom_init_set: invalid kind value ${kind} (|kind| must be 0..3)`,
  );
}
