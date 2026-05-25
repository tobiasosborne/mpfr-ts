/**
 * reference_ports/correct/mpfr_custom_move.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/stack_interface.c L53-L58):
 *   MPFR_MANT(x) = (mp_limb_t*) new_position
 *
 * In the immutable TS surface, MPFR values are frozen; mantissa is
 * stored inline (no pointer to rebind). The TS port is the IDENTITY
 * function.
 *
 * The `new_position` parameter from C is omitted entirely.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_custom_move(x: MPFR): MPFR {
  // Identity: no operational meaning for 'move' on an immutable inline
  // mantissa. Returning the input unchanged preserves the shape and
  // semantics callers should observe.
  return x;
}
