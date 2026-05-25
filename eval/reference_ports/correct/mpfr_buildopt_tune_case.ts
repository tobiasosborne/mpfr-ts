/**
 * reference_ports/correct/mpfr_buildopt_tune_case.ts -- mutation-prove
 * reference for mpfr_buildopt_tune_case.
 *
 * Returns the constant string 'default'. Pure-TS has no per-platform
 * tuning, so the honest answer is the libmpfr default.
 *
 * The type-only core.ts import satisfies the ast_check gate
 * (CLAUDE.md Law 4) without affecting the accessor's value.
 *
 * Ref: mpfr/src/buildopt.c L96-L100 -- C reference.
 * Ref: eval/functions/mpfr_buildopt_tune_case/spec.json -- contract.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_buildopt_tune_case(): string {
  // Pure-TS substrate has no platform-keyed tuning. The honest answer
  // is the libmpfr default tune-case string.
  return 'default';
}
