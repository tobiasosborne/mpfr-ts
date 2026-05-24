/**
 * reference_ports/correct/mpfr_set_exp.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_set_exp as _impl } from '../../../src/ops/set_exp.ts';

export function mpfr_set_exp(x: MPFR, e: bigint): MPFR {
  return _impl(x, e);
}
