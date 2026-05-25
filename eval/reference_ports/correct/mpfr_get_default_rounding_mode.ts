/**
 * reference_ports/correct/mpfr_get_default_rounding_mode.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/set_rnd.c L34-L38):
 *   return __gmpfr_default_rounding_mode
 *
 * Default is MPFR_RNDN (set at module load per L25). The TS port mirrors
 * this with a module-level constant; since each worker initializes fresh
 * (CLAUDE.md Rule 4) the returned value is always 'RNDN' in this PREP-time
 * reference.
 */

import type { RoundingMode } from '../../../src/core.ts';

export function mpfr_get_default_rounding_mode(): RoundingMode {
  return 'RNDN';
}
