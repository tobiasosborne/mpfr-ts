/**
 * reference_ports/correct/mpfr_mpn_sub_aux.ts — re-export of the production port.
 */

import { mpfr_mpn_sub_aux as _impl } from '../../../src/ops/mpn_sub_aux.ts';

export function mpfr_mpn_sub_aux(
  ap: bigint[], bp: bigint[], cy: bigint, extra: number,
): { result: bigint[]; borrow: bigint } {
  return _impl(ap, bp, cy, extra);
}
