/**
 * reference_ports/correct/mpfr_powerof2_raw.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_powerof2_raw as _impl } from '../../../src/internal/mpfr/powerof2_raw.ts';

export function mpfr_powerof2_raw(x: MPFR): boolean {
  return _impl(x);
}
