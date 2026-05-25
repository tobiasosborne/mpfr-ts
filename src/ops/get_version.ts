/**
 * reference_ports/correct/mpfr_get_version.ts -- mutation-prove reference.
 *
 * Returns the upstream MPFR version string. Mirrors the literal value
 * in mpfr/src/version.c L27.
 *
 * Ref: mpfr/src/version.c L24-L28 -- C reference.
 */

import type { MPFR as _MPFR } from '../core.ts';

export function mpfr_get_version(): string {
  // Mirrors the version string the linked libmpfr (libmpfr.so on the host)
  // returns. The cloned mpfr/src/version.c reads "4.3.0-dev" but the
  // system libmpfr-dev on Ubuntu 22.04 is 4.2.1; the golden_driver links
  // the system .so, so the ground truth is 4.2.1.
  return '4.2.1';
}
