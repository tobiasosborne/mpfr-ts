/**
 * reference_ports/correct/mpfr_init.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_init as _impl } from '../../../src/ops/init.ts';

export function mpfr_init(): MPFR {
  return _impl();
}
