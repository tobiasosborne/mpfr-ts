/**
 * reference_ports/broken/mpfr_free_pool.ts -- deliberately-buggy.
 *
 * **BUG: throws instead of returning null.** Single happy case fails.
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_free_pool(): null {
  throw new MPFRError('EDOMAIN', 'mpfr_free_pool: unexpected error');
}
