/**
 * reference_ports/correct/mpfr_mpn_cmpzero.ts — re-export of the production port.
 */

import { mpfr_mpn_cmpzero as _impl } from '../../../src/internal/mpfr/mpn_cmpzero.ts';

export function mpfr_mpn_cmpzero(ap: bigint[]): number {
  return _impl(ap);
}
