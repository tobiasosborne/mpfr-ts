/**
 * reference_ports/broken/mpfr_get_d.ts — deliberately-buggy mpfr_get_d.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md Step 8
 * and CLAUDE.md PIL.3 ("perturb the reference port, confirm the
 * composite drops below 0.95"). If this port scores composite > 0.3 on
 * the mpfr_get_d golden, the golden is too weak and the function is NOT
 * Pilot-passed.
 *
 * **Deliberately broken: always returns 0.0.**
 *
 * Every input — finite, NaN, ±Inf, signed zero, every rounding mode —
 * collapses to plain `0` (positive zero). This is a "structural stub"
 * bug: the agent wired up the signature and schema imports but forgot
 * to actually implement the conversion. The golden must reject this:
 *   - all NaN cases fail (expected NaN, got 0)
 *   - all ±Infinity cases fail (expected ±Infinity, got 0)
 *   - all finite cases fail (expected finite double, got 0)
 *   - all -0 cases fail (expected -0, got +0 — Object.is distinguishes)
 *   - only the +0 cases (under rounding modes that round to +0) pass.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.95 under mutation.
 * Ref: src/ops/get_d.ts — the correct version.
 */

import type { MPFR, RoundingMode } from '../../../src/core.ts';

export function mpfr_get_d(x: MPFR, rnd: RoundingMode): number {
  // BUG: every input collapses to +0. The real port would dispatch on
  // NaN / Inf / signed zero / overflow / underflow / finite normals and
  // round the source mantissa to the IEEE 754 binary64 grid. See
  // src/ops/get_d.ts for the correct algorithm.
  void x;
  void rnd;
  return 0.0;
}
