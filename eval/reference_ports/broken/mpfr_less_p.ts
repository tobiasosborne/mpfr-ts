/**
 * reference_ports/broken/mpfr_less_p.ts — deliberately-buggy mpfr_less_p.
 *
 * Used to mutation-prove the golden master per CLAUDE.md PIL.3. If this
 * port scores composite > 0.5 on the mpfr_less_p golden, the golden is
 * too weak and the function is NOT Pilot-passed.
 *
 * **Deliberately broken: returns `!correctAnswer` — the negation of the
 * correct predicate.** Every case answer is flipped. This is the
 * simplest mutation that still exercises the structural correctness of
 * the wire format + grader plumbing: the port still returns a boolean,
 * but the boolean is always wrong (apart from coincidences where
 * less_p and not-less_p collide, which they cannot — they're strict
 * negations).
 *
 * Behaviour:
 *   - a < b   : correct returns true,  broken returns false  (FAIL)
 *   - a == b  : correct returns false, broken returns true   (FAIL)
 *   - a > b   : correct returns false, broken returns true   (FAIL)
 *   - NaN     : correct returns false, broken returns true   (FAIL)
 *
 * So every case fails. corr/edge/mined all drop to 0; composite → 0.
 * That is comfortably below the 0.5 ceiling the mutation gate requires.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/less_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';

export function mpfr_less_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  // BUG: NEGATE the correct answer. Correct is `c === null ? false : c < 0`;
  // we return the opposite.
  return !(c === null ? false : c < 0);
}
