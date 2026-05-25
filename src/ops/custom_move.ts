/**
 * ops/custom_move.ts -- pure-TS port of MPFR's `mpfr_custom_move`.
 *
 * The C body is one line: `MPFR_MANT(x) = (mp_limb_t *) new_position`
 * -- rebinds the value's mantissa pointer to a caller-supplied buffer
 * (used when the caller has relocated their own storage). In the
 * immutable TS surface, MPFR values are frozen and the mantissa is
 * stored inline as a bigint; there is no pointer to rebind. The
 * `new_position` parameter from the C signature has no analogue and
 * is omitted.
 *
 * The TS port is therefore the identity function: takes an MPFR,
 * returns it unchanged. Per CLAUDE.md feedback_no_mutator_bait, the
 * port deliberately does not add machinery to make the function look
 * less trivial -- gaming the mutator gate destroys signal value, and
 * the carve-out (worklog 016) handles thin-surface ports cleanly.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/stack_interface.c L53-L58 -- C reference body.
 *   - src/core.ts L113-L135 -- MPFR shape (mantissa stored inline).
 */

import type { MPFR } from '../core.ts';

/**
 * Identity in the immutable surface. The C operation (rebinding a
 * mantissa pointer to relocated storage) has no analogue when the
 * mantissa lives inline as a frozen bigint.
 *
 * @mpfrName mpfr_custom_move
 *
 * @param x  An MPFR value.
 * @returns  The same value, unchanged.
 *
 * @example
 *   const a = posZero(53n);
 *   mpfr_custom_move(a) === a;  // true
 */
export function mpfr_custom_move(x: MPFR): MPFR {
  return x;
}
