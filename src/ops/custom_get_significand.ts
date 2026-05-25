/**
 * ops/custom_get_significand.ts -- pure-TS port of MPFR's
 * `mpfr_custom_get_significand`.
 *
 * The C body returns `MPFR_MANT(x)`, the raw `mp_limb_t*` pointer to the
 * mantissa storage. The TS schema stores the mantissa as a single
 * MSB-aligned bigint, so this port simply returns `x.mant`.
 *
 * Singular inputs (NaN/Inf/Zero) have no meaningful mantissa on either
 * side (C returns an uninitialised pointer, TS carries `0n` by
 * convention); the port fails fast on them per CLAUDE.md Rule 1.
 *
 * Ref: mpfr/src/stack_interface.c L39-L44 -- C reference body.
 * Ref: src/core.ts L113-L135 -- MPFR.mant; MSB-aligned bigint.
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Read the MSB-aligned mantissa of a normal MPFR value.
 *
 * @mpfrName mpfr_custom_get_significand
 *
 * @param x  A normal MPFR. Singular inputs throw EDOMAIN.
 * @returns  The mantissa as a bigint in `[2^(prec-1), 2^prec)`.
 */
export function mpfr_custom_get_significand(x: MPFR): bigint {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'mpfr_custom_get_significand: x must be an MPFR object');
  }
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_custom_get_significand: singular inputs out of scope (kind=${x.kind})`,
    );
  }
  return x.mant;
}
