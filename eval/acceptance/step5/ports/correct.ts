/**
 * Step-5 acceptance port (a): correct identity-on-input.
 *
 * The synthetic function `acceptanceFn(x: MPFR): Result` is defined as the
 * identity-on-input map, returning `{value: x, ternary: 0}`. This file is
 * the reference port: scenarios (b)–(e) are all controlled deviations from
 * it. Runner is expected to grade this `composite_correctness >= 0.95` on
 * the matching golden.
 *
 * Schema compliance: imports MPFR / Result / Ternary as types from
 * src/core.ts (Law 4) so the runner's ast_check passes pre-flight.
 *
 * This is RED-phase scaffolding (Step 5). The runner that grades it does
 * not exist yet; it lands in Step 6.
 */

import type { MPFR, Result, Ternary } from '../../../../src/core.ts';

/**
 * Identity-on-input. Returns the input value unchanged with an exact
 * ternary flag. Used as the "happy path" reference port for the runner
 * acceptance suite.
 */
export function acceptanceFn(x: MPFR): Result {
  const ternary: Ternary = 0;
  return { value: x, ternary };
}
