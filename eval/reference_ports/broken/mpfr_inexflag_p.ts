/**
 * reference_ports/broken/mpfr_inexflag_p.ts -- deliberately-buggy.
 *
 * **Collapses to a constant output**: always returns true regardless of
 * mask. Half of cases (those with INEXACT clear) fail; composite well
 * below 0.30.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_inexflag_p(mask: bigint): boolean {
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_inexflag_p: bad input`);
  }
  return true;
}
