/**
 * reference_ports/correct/mpfr_get_exp.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_get_exp as _impl } from '../../../src/ops/get_exp.ts';

export function mpfr_get_exp(x: MPFR): bigint {
  return _impl(x);
}
