/**
 * reference_ports/correct/mpfr_dump.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/dump.c L127-L131):
 *   mpfr_fdump(stdout, x)
 *
 * The TS port delegates to the shipped mpfr_fdump. Both functions
 * produce the same string; C writes to stdout, TS returns the string.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_fdump } from '../../../src/ops/fdump.ts';

export function mpfr_dump(x: MPFR): string {
  return mpfr_fdump(x);
}
