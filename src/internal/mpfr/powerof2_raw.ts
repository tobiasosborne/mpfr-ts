/**
 * mpfr/powerof2_raw.ts — pure-TS port of MPFR's `mpfr_powerof2_raw`.
 *
 * Substrate-class helper. A thin wrapper around `mpfr_powerof2_raw2`
 * that handles the boundary between the public MPFR value type and the
 * raw-mantissa predicate. The C function (mpfr/src/powerof2.c L30-L38)
 * is itself a single delegating call:
 *
 *     int mpfr_powerof2_raw (mpfr_srcptr x) {
 *         return mpfr_powerof2_raw2 (MPFR_MANT(x), MPFR_LIMB_SIZE(x));
 *     }
 *
 * The C side does NOT assert MPFR_IS_PURE_FP(x) (the assertion is
 * documented but disabled; the comment in powerof2.c L33-L36 explains
 * "we may call it with some wrong numbers"). The TS port matches that
 * behaviour: it inspects only the mantissa pattern via raw2 and treats
 * any non-normal kind as "not a power of 2" — there is no MSB pattern
 * to inspect for NaN/Inf/Zero, so the predicate is trivially false.
 *
 * C signature
 * -----------
 *
 *   int mpfr_powerof2_raw (mpfr_srcptr x);
 *
 * TS signature (this port)
 * ------------------------
 *
 *   mpfr_powerof2_raw(x: MPFR): boolean
 *
 * Semantics
 * ---------
 *
 *   - `x.kind === 'normal'`: delegate to `mpfr_powerof2_raw2(x.mant, x.prec)`.
 *     The result is `true` iff the only set bit of `x.mant` is at
 *     position `prec - 1` — i.e. `x.mant === 1n << (x.prec - 1n)`.
 *   - `x.kind === 'zero'`, `'inf'`, `'nan'`: returns `false`. None of
 *     these have a meaningful "power of 2" interpretation in the
 *     bit-pattern predicate sense.
 *
 *     Rationale for the non-normal branch: the C function looks at
 *     `MPFR_MANT(x)` regardless of kind. For singular values the
 *     mantissa storage is undefined/garbage; reading it and comparing
 *     to MPFR_LIMB_HIGHBIT is a coin-flip on the heap. The TS schema
 *     has `mant === 0n` for all singular kinds (src/core.ts L127-L128),
 *     so raw2 would return `false` consistently. Folding the kind
 *     check into the wrapper makes that explicit and removes the
 *     "what does C actually return here?" ambiguity from the goldens.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/powerof2.c L30-L38 — the C reference body.
 *   - mpfr/src/powerof2.c L40-L49 — mpfr_powerof2_raw2, the delegate.
 *   - src/internal/mpfr/powerof2_raw2.ts — substrate delegate.
 *   - src/core.ts L93-L135 — locked MPFR value model.
 *   - CLAUDE.md Law 3 — faithful substrate, idiomatic surface.
 */

import type { MPFR } from '../../../src/core.ts';
import { mpfr_powerof2_raw2 } from '../../../src/internal/mpfr/powerof2_raw2.ts';

/**
 * Returns true iff `|x|` is exactly a power of 2 — i.e. `x.kind === 'normal'`
 * and `x.mant` has only the MSB at position `prec - 1` set.
 *
 * Faithful TS port of `mpfr_powerof2_raw` from mpfr/src/powerof2.c L30-L38.
 *
 * @param x  An MPFR value of any kind.
 * @returns  `true` iff `x` is a normal value whose mantissa is exactly the MSB.
 *
 * @mpfrName mpfr_powerof2_raw
 */
export function mpfr_powerof2_raw(x: MPFR): boolean {
  // Singular kinds: no power-of-2 mantissa pattern to detect.
  // Ref: mpfr/src/powerof2.c L33-L36 — C reads MPFR_MANT(x) unguarded,
  // but in the TS schema mant === 0n for singular kinds, so raw2 would
  // return false anyway; we fold the check for explicitness.
  if (x.kind !== 'normal') {
    return false;
  }
  // Delegate to the bigint-level predicate.
  // Ref: mpfr/src/powerof2.c L37 — C: `return mpfr_powerof2_raw2(MPFR_MANT(x), MPFR_LIMB_SIZE(x));`
  return mpfr_powerof2_raw2(x.mant, x.prec);
}
