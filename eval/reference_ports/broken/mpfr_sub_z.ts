/**
 * reference_ports/broken/mpfr_sub_z.ts -- deliberately-buggy port.
 *
 * **Deliberately broken: always returns posZero(prec)**. Per worklog
 * 018, "collapse to constant" beats narrow perturbation.
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: composite <= 0.5.
 *
 * Ref: eval/reference_ports/correct/mpfr_sub_z.ts -- the correct version.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../../../src/core.ts';

export function mpfr_sub_z(
  x: MPFR,
  z: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  if (typeof z !== 'bigint') {
    throw new MPFRError('EPREC', `z must be bigint, got ${typeof z}`);
  }
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
  void x;
  void z;
  return { value: posZero(prec), ternary: 0 };
}
