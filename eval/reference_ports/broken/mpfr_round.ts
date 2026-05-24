/**
 * reference_ports/broken/mpfr_round.ts — deliberately-buggy mpfr_round.
 *
 * **Deliberately broken: rounds toward zero (truncates) instead of
 * to-nearest-ties-away.** Drops the fractional part regardless of
 * magnitude. Mirrors a plausible agent error: "I confused MPFR_RNDNA
 * with MPFR_RNDZ when reading rint.c."
 *
 * Behaviour:
 *   - NaN/Inf/Zero: matches correct.
 *   - 0.5 → 0 (correct says 1).
 *   - 0.7 → 0 (correct says 1).
 *   - 2.5 → 2 (correct says 3).
 *   - 1.6 → 1 (correct says 2).
 *
 * The behavioural difference from correct is "any x with |frac| >= 0.5
 * (the round-to-nearest tie-or-up case)" plus all |x| < 1 with |x| >=
 * 0.5 (which round maps to ±1 but broken maps to ±0). Roughly half of
 * random fractional inputs.
 *
 * Ref: src/ops/round.ts — the correct version.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_trunc as _truncImpl } from '../../../src/ops/trunc.ts';

export function mpfr_round(x: MPFR, prec: bigint): Result {
  // BUG: should round to nearest (RNDNA). Truncates instead.
  return _truncImpl(x, prec);
}
