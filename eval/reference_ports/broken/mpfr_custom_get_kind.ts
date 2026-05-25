/**
 * reference_ports/broken/mpfr_custom_get_kind.ts -- deliberately-buggy.
 *
 * **BUG: swaps INF_KIND (1) and REGULAR_KIND (3).** Plausible mistake:
 * agent confuses which is which since the enum order in mpfr.h goes
 * NAN(0)/INF(1)/ZERO(2)/REGULAR(3) but the C body lists REGULAR-then-INF.
 * Every INF and every normal case mismatches.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const NAN_KIND = 0;
const INF_KIND = 3;       /* BUG: should be 1 */
const ZERO_KIND = 2;
const REGULAR_KIND = 1;   /* BUG: should be 3 */

export function mpfr_custom_get_kind(x: MPFR): number {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'mpfr_custom_get_kind: x must be MPFR');
  }
  switch (x.kind) {
    case 'nan': return NAN_KIND;
    case 'inf': return INF_KIND * x.sign;
    case 'zero': return ZERO_KIND * x.sign;
    case 'normal': return REGULAR_KIND * x.sign;
    default:
      throw new MPFRError('EDOMAIN', `mpfr_custom_get_kind: unknown kind`);
  }
}
