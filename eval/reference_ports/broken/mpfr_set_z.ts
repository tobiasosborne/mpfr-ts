/**
 * reference_ports/broken/mpfr_set_z.ts — deliberately-buggy mpfr_set_z.
 *
 * **Deliberately broken: always returns posZero** — drops every input
 * to +0 at the target precision, ternary 0. Only the z=0 cases happen
 * to pass; everything else fails on kind, sign, exp, and mant.
 *
 * The "always zero" pattern is the simplest stub-shaped mistake (the
 * agent forgets to fill in the body) and is broad enough that the
 * composite stays below the gate even with the z=0 happy cases passing.
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: composite ≤ 0.5.
 *
 * Ref: src/ops/set_z.ts — the correct version.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../../../src/core.ts';

function validateArgs(z: bigint, prec: bigint, rnd: RoundingMode): void {
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
}

export function mpfr_set_z(
  z: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(z, prec, rnd);
  // BUG: always returns posZero, ignoring z.
  void z;
  return { value: posZero(prec), ternary: 0 };
}
