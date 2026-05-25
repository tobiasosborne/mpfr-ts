/**
 * reference_ports/broken/mpfr_get_version.ts -- deliberately-buggy.
 *
 * **Collapses output to a constant wrong string.** Single golden case
 * fails on strict string equality.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_get_version(): string {
  return 'unknown';
}
