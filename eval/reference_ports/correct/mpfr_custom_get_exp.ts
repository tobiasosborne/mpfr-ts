/**
 * reference_ports/correct/mpfr_custom_get_exp.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/stack_interface.c L46-L51): return MPFR_EXP(x).
 * For NORMAL inputs the TS port simply returns x.exp (a bigint).
 *
 * Singular inputs are out of scope (the C function returns sentinel
 * exponents; the TS schema has no sentinels). The golden_driver does
 * not test singular inputs; the port throws on them to fail-fast.
 *
 * Ref: mpfr/src/stack_interface.c L46-L51 -- C reference.
 * Ref: src/core.ts L113-L135 -- MPFR.exp; 0n for singulars.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_custom_get_exp(x: MPFR): bigint {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_custom_get_exp: x must be an MPFR object`,
    );
  }
  if (x.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_custom_get_exp: singular inputs out of scope (kind=${x.kind})`,
    );
  }
  return x.exp;
}
