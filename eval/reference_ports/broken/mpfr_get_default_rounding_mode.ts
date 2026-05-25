/**
 * reference_ports/broken/mpfr_get_default_rounding_mode.ts -- deliberately-buggy.
 *
 * **BUG: returns 'RNDZ' instead of the default 'RNDN'.** Single happy
 * case fails -> composite=0.
 */

import type { RoundingMode } from '../../../src/core.ts';

export function mpfr_get_default_rounding_mode(): RoundingMode {
  return 'RNDZ';
}
