/**
 * reference_ports/correct/mpfr_roundeven.ts -- calibration reference.
 *
 * mpfr_roundeven(r, u) is a one-line wrapper for mpfr_rint(r, u, RNDN)
 * (mpfr/src/rint.c L308-L312). The idiomatic TS surface fixes the mode
 * to ties-to-even by delegating to the RNDN-mode engine; there is no
 * rnd parameter.
 *
 * This is the PREP-step calibration port: it MUST score composite 1.0
 * against the golden. The production port (written later by the porter
 * model) may inline the engine; this reference simply delegates to the
 * already-shipped src/ops/rint.ts, mirroring the C wrapper exactly.
 *
 * Ref: mpfr/src/rint.c L308-L312 -- mpfr_roundeven := mpfr_rint(.,.,RNDN).
 * Ref: src/ops/rint.ts -- the engine.
 * Ref: src/core.ts -- locked MPFR / Result types.
 * Ref: CLAUDE.md PIL.3 -- mutation-prove the golden against this file.
 */

import type { MPFR, Result } from '../../../src/core.ts';
import { mpfr_rint as _impl } from '../../../src/ops/rint.ts';

export function mpfr_roundeven(x: MPFR, prec: bigint): Result {
  // Ref: mpfr/src/rint.c L311 -- return mpfr_rint(r, u, MPFR_RNDN).
  return _impl(x, prec, 'RNDN');
}
