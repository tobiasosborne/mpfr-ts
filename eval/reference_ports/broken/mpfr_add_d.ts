/**
 * reference_ports/broken/mpfr_add_d.ts — deliberately-buggy mpfr_add_d.
 *
 * **Multi-bug perturbation (per worklog 006 #6 — single-bug perturbations
 * land in the 0.45-0.55 mutation-prove danger zone; multi-bug pushes
 * cleanly below 0.30):**
 *
 *   1. Calls mpfr_sub instead of mpfr_add — wrong operation across all
 *      cases.
 *   2. Negates the ternary on the way out — wrong direction even if a
 *      caller checked just the sign.
 *
 * Effect: nearly every case mismatches. Composite should drop to ~0.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/add_d.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_sub } from '../../../src/ops/sub.ts';
import { mpfr_set_d } from '../../../src/ops/set_d.ts';

export function mpfr_add_d(
  b: MPFR,
  c: number,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Convert c → MPFR at prec=53 (exact for any finite/Inf/NaN double).
  const dMpfr = mpfr_set_d(c, 53n, rnd).value;
  // BUG 1: subtract instead of add.
  const r = mpfr_sub(b, dMpfr, prec, rnd);
  // BUG 2: negate ternary.
  return { value: r.value, ternary: (-r.ternary) as Ternary };
}
