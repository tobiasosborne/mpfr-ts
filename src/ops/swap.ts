/**
 * ops/swap.ts — pure-TS port of MPFR's `mpfr_swap`.
 *
 * Idiomatic surface-class function. The C reference mutates both operands
 * in-place, swapping all four struct fields (precision, sign, exponent, and
 * mantissa pointer). The TS surface is immutable: we accept two `MPFR` values
 * and return `{ a: b, b: a }` — the same logical swap expressed as a
 * value-returning function.
 *
 * C signature:
 *
 *   void mpfr_swap(mpfr_ptr u, mpfr_ptr v)
 *
 * TS signature (this port):
 *
 *   mpfr_swap(a, b) -> { a: MPFR; b: MPFR }
 *
 * The return object has field `a` set to the original `b` and field `b` set
 * to the original `a`. Every field (kind, sign, prec, exp, mant) round-trips
 * verbatim — no rounding, no precision change, no allocation. NaN, Inf, Zero,
 * and normal values all pass through without modification.
 *
 * The NaN sentinel is canonicalised by the schema at construction time
 * (kind=nan ⇒ sign=1, prec=0n, exp=0n, mant=0n); swap does not re-canonicalise
 * because the inputs are already schema-valid immutable values by contract.
 *
 * Refs:
 *   - mpfr/src/swap.c L26-L53 — C reference body; swaps prec, sign, exp, mant.
 *   - mpfr/src/mpfr.h L247-L253 — __mpfr_struct layout (four fields touched).
 *   - src/core.ts — locked MPFR shape; immutable, so we return a new object.
 *   - CLAUDE.md Law 3 — faithful substrate, idiomatic surface (value-level TS).
 */

import type { MPFR } from "../core.ts";

/** Return type for {@link mpfr_swap}: the two inputs exchanged. */
export interface SwapResult {
  readonly a: MPFR;
  readonly b: MPFR;
}

/**
 * Swap two MPFR values.
 *
 * Returns a fresh object `{ a: b, b: a }`. The contents of each value are
 * preserved exactly — no rounding, no precision loss, no re-normalisation.
 *
 * Equivalent to C's `mpfr_swap(u, v)` (which mutates in-place), adapted to
 * the immutable TS surface API.
 *
 * @mpfrName mpfr_swap
 * @param a First operand.
 * @param b Second operand.
 * @returns Object with the two operands exchanged.
 *
 * Ref: mpfr/src/swap.c L26-L53 — field-by-field swap of prec, sign, exp,
 *   mant. In TS we just return (b, a) since both inputs are already immutable
 *   value objects; no field-by-field copy is needed.
 */
export function mpfr_swap(a: MPFR, b: MPFR): SwapResult {
  // Ref: mpfr/src/swap.c L26-L53 — C reads MPFR_PREC/SIGN/EXP/MANT from u
  //   and writes them to v (and vice versa). In TS the values are immutable
  //   objects, so the entire swap reduces to returning the inputs in the
  //   opposite positions. No field manipulation needed.
  return { a: b, b: a };
}
