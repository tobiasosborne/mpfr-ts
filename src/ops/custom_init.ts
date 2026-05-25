/**
 * ops/custom_init.ts -- pure-TS port of MPFR's `mpfr_custom_init`.
 *
 * The C body is a literal no-op (`return;`) -- it accepts a
 * caller-allocated mantissa buffer plus a precision and does nothing,
 * since the actual value-population happens via `mpfr_custom_init_set`
 * or another setter. The buffer parameter exists purely so the caller
 * can pre-allocate storage outside the MPFR heap.
 *
 * The immutable TS surface has no caller-managed mantissa buffers --
 * every MPFR value owns its mantissa as a bigint. Following the
 * `mpfr_init2` precedent (already shipped), this port returns a
 * fresh `+0` at the requested precision; the `mantissa` argument from
 * the C signature is omitted entirely. Callers that would have called
 * `mpfr_custom_init(buf, prec)` then `mpfr_custom_init_set(...)` now
 * just call `mpfr_custom_init_set(...)` directly.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/stack_interface.c L32-L37 -- C reference body.
 *   - eval/functions/mpfr_init2/spec.json -- sister init function with
 *     the same posZero-return contract.
 *   - src/core.ts L278-L281 -- posZero constructor.
 */

import type { MPFR } from '../core.ts';
import { posZero } from '../core.ts';

/**
 * Initialise a fresh MPFR value at the given precision.
 *
 * @mpfrName mpfr_custom_init
 *
 * @param prec  Precision in bits, in `[PREC_MIN, PREC_MAX]`.
 * @returns     A fresh `+0` MPFR at the requested precision, ready to
 *              be reassigned by a subsequent setter.
 *
 * @example
 *   mpfr_custom_init(53n);  // posZero at 53 bits
 */
export function mpfr_custom_init(prec: bigint): MPFR {
  // posZero performs the prec validation via assertPrec in core.ts.
  return posZero(prec);
}
