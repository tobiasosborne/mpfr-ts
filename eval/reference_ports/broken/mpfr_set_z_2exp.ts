/**
 * reference_ports/broken/mpfr_set_z_2exp.ts -- deliberately-buggy port.
 *
 * **Deliberately broken: always returns posZero** -- drops every
 * input to +0 at the target precision, ternary 0. Only the z=0 cases
 * happen to pass; every nonzero z fails on kind, sign, exp, and mant
 * regardless of e.
 *
 * Per worklog 018 lesson, this "collapse to constant" pattern beats
 * "perturb one branch" because narrow perturbations leave too many
 * cases passing (goldens typically exercise the other branches
 * uniformly).
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: composite <= 0.5.
 *
 * Ref: eval/reference_ports/correct/mpfr_set_z_2exp.ts -- the correct
 *   version.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../../../src/core.ts';

function validateArgs(
  z: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  if (typeof z !== 'bigint') {
    throw new MPFRError('EPREC', `z must be bigint, got ${typeof z}`);
  }
  if (typeof e !== 'bigint') {
    throw new MPFRError('EPREC', `e must be bigint, got ${typeof e}`);
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
}

export function mpfr_set_z_2exp(
  z: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(z, e, prec, rnd);
  // BUG: always returns posZero, ignoring z and e.
  void z;
  void e;
  return { value: posZero(prec), ternary: 0 };
}
