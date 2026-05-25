/**
 * reference_ports/broken/mpfr_buildopt_tune_case.ts -- deliberately-buggy
 * mpfr_buildopt_tune_case.
 *
 * **Deliberately broken: returns a different string** ('haswell' instead
 * of 'default'). The golden expects 'default'; any other string trips
 * the strict `===` comparison in the grader's scalar branch and the
 * single happy case fails, dropping composite to 0.0.
 *
 * Mutation-prove gap: correct port -> 1.0; broken port -> 0.0. Clean
 * 1.0 gap, well outside the 0.45-0.55 danger zone.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file -- the bug is the point.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_buildopt_tune_case(): string {
  // BUG: should be 'default'. Returning a real-but-wrong tune-case
  // string so the single happy case fails on the wire.
  return 'haswell';
}
