/**
 * reference_ports/correct/mpfr_get_z.ts — re-export of the production port.
 *
 * See src/ops/get_z.ts for the algorithm.
 *
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { MPFR, RoundingMode } from '../../../src/core.ts';
import { mpfr_get_z as _impl } from '../../../src/ops/get_z.ts';

export function mpfr_get_z(x: MPFR, rnd: RoundingMode): bigint {
  return _impl(x, rnd);
}
