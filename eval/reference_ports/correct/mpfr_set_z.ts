/**
 * reference_ports/correct/mpfr_set_z.ts — re-export of the production port.
 *
 * See src/ops/set_z.ts for the algorithm.
 *
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import { mpfr_set_z as _impl } from '../../../src/ops/set_z.ts';

export function mpfr_set_z(z: bigint, prec: bigint, rnd: RoundingMode): Result {
  return _impl(z, prec, rnd);
}
