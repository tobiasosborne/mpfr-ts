/**
 * reference_ports/broken/mpfr_get_z.ts — deliberately-buggy mpfr_get_z.
 *
 * **Deliberately broken: always returns 0n** — the simplest stub
 * mistake. Only the ±0 cases happen to pass; every nonzero input
 * fails the value comparison.
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: composite ≤ 0.5.
 *
 * Ref: src/ops/get_z.ts — the correct version.
 */

import type { MPFR, RoundingMode } from '../../../src/core.ts';
import { MPFRError, validate } from '../../../src/core.ts';

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

export function mpfr_get_z(x: MPFR, rnd: RoundingMode): bigint {
  validateRnd(rnd);
  validate(x);

  // BUG: always returns 0n, ignoring x. We still throw on NaN/Inf so
  // the divergence shape matches the correct version (i.e. the bug is
  // narrow: the value calculation, not the special-value handling).
  if (x.kind === 'nan') {
    throw new MPFRError('EPREC', 'mpfr_get_z: NaN');
  }
  if (x.kind === 'inf') {
    throw new MPFRError('EPREC', 'mpfr_get_z: Inf');
  }
  return 0n;
}
