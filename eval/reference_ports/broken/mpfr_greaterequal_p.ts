/**
 * reference_ports/broken/mpfr_greaterequal_p.ts — deliberately-buggy mpfr_greaterequal_p.
 *
 * **Deliberately broken: drops the equality case — returns `c > 0`
 * instead of `c >= 0`.** Same shape as the broken lessequal_p
 * mutation. See `./mpfr_lessequal_p.ts` for the rationale.
 *
 * Behaviour:
 *   - a < b   : correct returns false, broken returns false  (PASS)
 *   - a == b  : correct returns true,  broken returns false  (FAIL)
 *   - a > b   : correct returns true,  broken returns true   (PASS)
 *   - NaN     : correct returns false, broken returns false  (PASS)
 *
 * Every `a == b` case fails — golden's equality mass forces composite
 * below the 0.5 mutation-gate ceiling.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/greaterequal_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { compareMPFR } from '../../../src/internal/mpfr/cmp_raw.ts';

export function mpfr_greaterequal_p(a: MPFR, b: MPFR): boolean {
  const c = compareMPFR(a, b);
  // BUG: should be `c >= 0`. Use strict `c > 0` instead — drops equality.
  return c === null ? false : c > 0;
}
