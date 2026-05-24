/**
 * ops/get_prec.ts — pure-TS port of MPFR's `mpfr_get_prec`.
 *
 * Return the precision (in bits) of an {@link MPFR} value as a `bigint`.
 *
 * C signature
 * -----------
 *
 *   mpfr_prec_t mpfr_get_prec(mpfr_srcptr x);
 *
 *   Returns the precision of x, which is x->_mpfr_prec (accessed via
 *   the MPFR_PREC macro).
 *
 *   Ref: mpfr/src/set_prec.c L54–L59:
 *
 *     #undef mpfr_get_prec
 *     mpfr_prec_t
 *     mpfr_get_prec (mpfr_srcptr x)
 *     {
 *       return MPFR_PREC(x);
 *     }
 *
 * TS signature
 * ------------
 *
 *   mpfr_get_prec(x: MPFR): bigint;
 *
 *   - Takes a single immutable {@link MPFR} from `src/core.ts`.
 *   - Returns `x.prec` as a `bigint`.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * MPFR's C side returns `mpfr_prec_t` (a signed integer type, platform-
 * dependent but at least 32 bits). The TS port returns `bigint` to stay
 * consistent with the locked schema's convention that all precision values
 * are always `bigint`.
 *
 * NaN-precision divergence: The TS schema folds every NaN to the canonical
 * `NAN_VALUE` with `prec=0n` (src/core.ts L103–L107, MPFR.prec docs), while
 * libmpfr keeps the originating precision on its NaN. So
 * `mpfr_get_prec(NAN_VALUE)` returns `0n` in TS, but returns whatever prec
 * the C NaN was constructed at in libmpfr. The golden driver emits the
 * TS-expected `0` for NaN inputs (driver constructs NaN-shaped wire records
 * by hand), so the divergence is exercised, not papered over.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/set_prec.c L54–L59 — the C reference; trivial wrapper
 *     around `MPFR_PREC(x)`.
 *   - mpfr/src/mpfr.h L247–L253 — `__mpfr_struct` layout: `_mpfr_prec`
 *     field that `MPFR_PREC` reads.
 *   - src/core.ts L113–L135 — `MPFR.prec` field: bigint, >= 1 for
 *     non-NaN, 0n for NaN (the canonical NAN_VALUE sentinel).
 *   - src/core.ts L103–L107 — NaN sentinel convention: prec=0n.
 *   - eval/functions/mpfr_get_prec/spec.json — function-specific contract.
 */

import type { MPFR } from "../core.ts";

/**
 * Return the precision of `x` in bits.
 *
 * @mpfrName mpfr_get_prec
 *
 * @param x  The {@link MPFR} value to inspect.
 * @returns  `x.prec` as a `bigint`. For normal, zero, and inf kinds this
 *           is the precision in bits (>= 1). For NaN (the canonical
 *           `NAN_VALUE`) this returns `0n` — see "Divergence from C → TS"
 *           in the module docstring.
 *
 * @example
 *   mpfr_get_prec(setD(3.14, 53n, 'RNDN').value);  // 53n
 *   mpfr_get_prec(posZero(24n));                    // 24n
 *   mpfr_get_prec(posInf(113n));                    // 113n
 *   mpfr_get_prec(NAN_VALUE);                       // 0n  (TS sentinel)
 */
export function mpfr_get_prec(x: MPFR): bigint {
  // Ref: mpfr/src/set_prec.c L57–L58 — `return MPFR_PREC(x);`
  // `MPFR_PREC(x)` expands to `((x)->_mpfr_prec)`, directly reading the
  // precision field of the struct. In the TS schema this is `x.prec`, a
  // bigint field on the immutable MPFR record.
  //
  // NaN: the TS NAN_VALUE has `prec=0n` by convention (src/core.ts L103–107);
  // x.prec handles this correctly without a special case — the field is 0n
  // for NaN and >= 1n for all other kinds.
  return x.prec;
}
