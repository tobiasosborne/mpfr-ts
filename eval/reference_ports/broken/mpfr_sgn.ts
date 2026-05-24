/**
 * reference_ports/broken/mpfr_sgn.ts — deliberately-buggy mpfr_sgn.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3.
 *
 * **Deliberately broken: returns `x.sign` always (ignores `kind`).**
 * Zeros, which the correct port collapses to `0`, return `±1` here.
 * The bug shape mirrors a plausible agent error: "I forgot that
 * mpfr_sgn treats ±0 differently from arithmetic ops — `sgn(±0) = 0`,
 * not the sign-of-zero bit."
 *
 * Behaviour:
 *   - NaN → throws EDOMAIN (matches correct; same kind-check first).
 *   - zero → returns x.sign (1 or -1) instead of 0. **Bug surfaces.**
 *   - inf → returns x.sign (matches correct).
 *   - normal → returns x.sign (matches correct).
 *
 * The bug only surfaces on zero cases — those must be heavily
 * represented in the golden to push composite below 0.5.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: src/ops/sgn.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export function mpfr_sgn(x: MPFR): number {
  if (x.kind === 'nan') {
    throw new MPFRError('EDOMAIN', 'mpfr_sgn(broken): NaN operand');
  }
  // BUG: should return 0 for kind === 'zero'. Falls through to x.sign.
  return x.sign;
}
