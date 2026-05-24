/**
 * reference_ports/broken/mpfr_sqrt1n.ts -- deliberately-buggy mpfr_sqrt1n.
 *
 * **Multi-bug perturbation (per worklog 006 #6 -- single-bug perturbations
 * land in the 0.45-0.55 mutation-prove danger zone; multi-bug pushes
 * cleanly below 0.30):**
 *
 *   1. Wrong output precision -- always returns at prec(u) instead of the
 *      requested prec=64. This produces a value with the wrong .prec field
 *      AND a structurally-different (smaller) mantissa on every case where
 *      u.prec < 64 (~25/60 fuzz + most edge cases). On u.prec=64 cases this
 *      bug is a no-op.
 *
 *   2. Flip sticky-bit-driven rounding -- negate the ternary on every
 *      inexact result (-1 <-> +1). Breaks every irrational + most happy
 *      inputs at any rnd != exact-passthrough.
 *
 *   3. Invert directional rounding modes (RNDU <-> RNDD, RNDA -> RNDZ).
 *      Compounds with #2 on the edge cases that exercise directional
 *      polarities.
 *
 * Expected mutation-prove score: composite well below 0.30 on the generated
 * golden -- bug #1 breaks ~all u.prec<64 cases (mostly via .prec field
 * mismatch); bugs #2 and #3 break ~all inexact-rounding cases. Combined
 * coverage is near-total.
 *
 * NOT used in production. Do NOT fix this file.
 *
 * Ref: src/ops/sqrt.ts -- the correct delegate.
 * Ref: docs/worklog/006-broken-port-calibration.md -- multi-bug pattern.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { mpfr_sqrt } from '../../../src/ops/sqrt.ts';

function invertRnd(rnd: RoundingMode): RoundingMode {
  // BUG 3: invert directional rounding.
  if (rnd === 'RNDU') return 'RNDD';
  if (rnd === 'RNDD') return 'RNDU';
  if (rnd === 'RNDA') return 'RNDZ';
  return rnd;
}

function negateTernary(t: Ternary): Ternary {
  // BUG 2: flip -1 <-> +1; leave 0 alone.
  if (t === 1) return -1;
  if (t === -1) return 1;
  return 0;
}

export function mpfr_sqrt1n(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  void prec;  // explicitly drop -- BUG 1 uses u.prec instead.
  // BUG 1: use u.prec instead of the requested prec. For u.kind='nan' or
  // u with .prec == 0n this would throw; the golden filters those out so
  // we just clamp to a safe value.
  const wrongPrec = u.prec >= 1n ? u.prec : 1n;
  const result = mpfr_sqrt(u, wrongPrec, invertRnd(rnd));
  return {
    value: result.value,
    ternary: negateTernary(result.ternary),
  };
}
