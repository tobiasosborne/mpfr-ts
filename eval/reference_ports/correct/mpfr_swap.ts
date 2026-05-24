/**
 * reference_ports/correct/mpfr_swap.ts — re-export wrapper.
 *
 * Delegates to the production port. Do NOT duplicate.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_swap as _impl } from '../../../src/ops/swap.ts';

export function mpfr_swap(
  a: MPFR,
  b: MPFR,
): { readonly a: MPFR; readonly b: MPFR } {
  return _impl(a, b);
}
