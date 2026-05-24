/**
 * reference_ports/correct/mpfr_sqrt1n.ts -- re-export of the production port.
 *
 * The unified TS mpfr_sqrt in src/ops/sqrt.ts handles all prec windows with
 * a single bigint isqrt; the C-side mpfr_sqrt1n fast path (mpfr/src/sqrt.c
 * L220-L346) is a perf-only specialization for prec(r) == 64 with prec(u) <=
 * 64. The dispatcher precondition is enforced by the golden driver, so
 * delegating to mpfr_sqrt produces byte-identical output.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: docs/reports/010-shadow-trial.md -- same pattern as mpfr_div_2 / mpfr_sqrt1.
 *
 * NOTE TO SONNET PORT WAVE: when the production port lands at
 * src/internal/mpfr/sqrt1n.ts (or wherever the eventual decision lives),
 * update this import path. For now, the unified src/ops/sqrt.ts is the
 * byte-identical delegate.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_sqrt } from '../../../src/ops/sqrt.ts';

export function mpfr_sqrt1n(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  return mpfr_sqrt(u, prec, rnd);
}
