/**
 * reference_ports/correct/mpfr_custom_init.ts -- mutation-prove reference.
 *
 * Algorithm: matches mpfr_init2's contract (posZero(prec)). The C
 * mpfr_custom_init is literally a no-op (mpfr/src/stack_interface.c
 * L32-L37); the TS port produces a fresh +0 at the requested precision
 * so the value passes validate() without post-processing.
 */

import type { MPFR } from '../../../src/core.ts';
import { posZero } from '../../../src/core.ts';

export function mpfr_custom_init(prec: bigint): MPFR {
  // posZero performs the prec validation via assertPrec in core.ts.
  return posZero(prec);
}
