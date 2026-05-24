/**
 * reference_ports/correct/mpfr_print_rnd_mode.ts — re-export wrapper.
 *
 * Imports schema types for Law 4 compliance, delegates to the
 * production port. Do NOT duplicate.
 *
 * Ref: docs/PILOT_PLAN.md Step 7.
 * Ref: CLAUDE.md PIL.3.
 */

import type { RoundingMode } from '../../../src/core.ts';
import { mpfr_print_rnd_mode as _impl } from '../../../src/ops/print_rnd_mode.ts';

export function mpfr_print_rnd_mode(rnd: RoundingMode): { readonly name: string } {
  return _impl(rnd);
}
