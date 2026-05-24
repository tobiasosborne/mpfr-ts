/**
 * reference_ports/correct/mpfr_mpn_cmp_aux.ts — re-export of the production port.
 */

import { mpfr_mpn_cmp_aux as _impl } from '../../../src/internal/mpfr/mpn_cmp_aux.ts';

export function mpfr_mpn_cmp_aux(ap: bigint[], bp: bigint[], extra: number): number {
  return _impl(ap, bp, extra);
}
