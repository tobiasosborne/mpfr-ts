/**
 * reference_ports/broken/mpfr_equal_p.ts — deliberately-buggy mpfr_equal_p.
 *
 * **Deliberately broken: returns `false` unconditionally.** A plausible
 * agent error: the stub before the real implementation got committed.
 *
 * Behaviour:
 *   - a == b  : correct returns true,  broken returns false  (FAIL)
 *   - a != b  : correct returns false, broken returns false  (PASS)
 *   - NaN     : correct returns false, broken returns false  (PASS)
 *
 * Equality cases dominate the edge+mined buckets (signed zero, same-Inf,
 * same-value-different-prec, etc.); composite drops below 0.5 once
 * those fail. The unconditional-`false` mutation is preferable to
 * negation because it gives a CLEAR gap signal — the `equal_p` golden
 * lacks the +0/-0 mass that `less_p` enjoys, so the simpler mutation
 * needs no calibration.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/equal_p.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { validate } from '../../../src/core.ts';

export function mpfr_equal_p(a: MPFR, b: MPFR): boolean {
  // Still validate inputs so the broken port matches the shape of the
  // correct one (i.e. doesn't accidentally pass cases where the correct
  // port throws on a malformed value). Without this, malformed-input
  // adversarial cases would be n_pass for the broken port (returns
  // false) but n_throw for the correct port — narrowing the gap.
  validate(a);
  validate(b);
  // BUG: unconditional false.
  return false;
}
