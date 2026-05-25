/**
 * reference_ports/correct/mpfr_custom_get_kind.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/stack_interface.c L91-L103):
 *   - NaN     -> NAN_KIND (0)
 *   - Inf     -> INF_KIND (1)  * sign
 *   - Zero    -> ZERO_KIND (2) * sign
 *   - Regular -> REGULAR_KIND (3) * sign
 *
 * Bit-for-bit identical to MPFR's mpfr.h enum.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const NAN_KIND = 0;
const INF_KIND = 1;
const ZERO_KIND = 2;
const REGULAR_KIND = 3;

export function mpfr_custom_get_kind(x: MPFR): number {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'mpfr_custom_get_kind: x must be MPFR');
  }
  switch (x.kind) {
    case 'nan':
      return NAN_KIND;
    case 'inf':
      return INF_KIND * x.sign;
    case 'zero':
      return ZERO_KIND * x.sign;
    case 'normal':
      return REGULAR_KIND * x.sign;
    default:
      throw new MPFRError(
        'EDOMAIN',
        `mpfr_custom_get_kind: unknown kind '${String(x.kind)}'`,
      );
  }
}
