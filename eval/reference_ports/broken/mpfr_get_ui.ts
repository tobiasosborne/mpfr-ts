/**
 * reference_ports/broken/mpfr_get_ui.ts — deliberately-buggy mpfr_get_ui.
 *
 * **Deliberately broken: returns 0n for every input.** Same shape as
 * broken/mpfr_get_si.ts.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: src/ops/get_ui.ts — the correct version.
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

export function mpfr_get_ui(x: MPFR, rnd: RoundingMode): bigint {
  validateRnd(rnd);
  validate(x);
  // BUG: always return 0n.
  return 0n;
}
