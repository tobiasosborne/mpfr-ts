/**
 * reference_ports/broken/mpfr_zero_p.ts — deliberately-buggy mpfr_zero_p.
 *
 * **Deliberately broken: returns `x.kind === 'normal'` instead of
 * `x.kind === 'zero'`.** A plausible "agent confused finite-vs-zero"
 * mutation.
 *
 * Behaviour vs. correct port:
 *
 *   - zero : correct true,  broken false (FAIL)
 *   - norm : correct false, broken true  (FAIL)
 *   - Inf  : correct false, broken false (PASS by coincidence)
 *   - NaN  : correct false, broken false (PASS by coincidence)
 *
 * The golden has both zero (every signed-zero pair, edge cases) and
 * normal-output mass (most happy/fuzz cases are finite normal). Both
 * flip; composite drops below 0.5.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/zero_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_zero_p(x: MPFR): boolean {
  // BUG: wrong kind. Should be 'zero', is 'normal'.
  return x.kind === 'normal';
}
