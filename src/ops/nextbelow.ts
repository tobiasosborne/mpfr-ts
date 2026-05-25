/**
 * ops/nextbelow.ts -- pure-TS port of MPFR's `mpfr_nextbelow`.
 *
 * Step `x` one ULP toward `-infinity` at x's own precision. This is the
 * IEEE 754-2008 `nextDown` operation restricted to a single MPFR
 * precision. The mirror of {@link mpfr_nextabove}: same NaN handling, but
 * the sign-based dispatch is INVERTED. No rounding occurs, so the TS
 * surface returns a fresh {@link MPFR} rather than a `Result` pair.
 *
 * The 14-line C body is pure sign-dispatch over two already-ported
 * helpers:
 *
 *   - NaN          -> {@link NAN_VALUE} (C sets MPFR_FLAGS_NAN; not modelled).
 *   - sign === -1  -> {@link mpfr_nexttoinf}  (stepping below a negative
 *                     value moves further from zero, toward -inf).
 *   - otherwise    -> {@link mpfr_nexttozero} (stepping below non-negative
 *                     moves toward zero, with sign-flip on +/-0).
 *
 * Signed-zero asymmetry: nextbelow(+0) routes via nexttozero and yields
 * -smallest (sign flipped); nextbelow(-0) routes via nexttoinf and
 * yields -smallest (sign preserved). Both directions land on -smallest,
 * matching IEEE 754 `nextDown(+/-0)`.
 *
 * Ref: mpfr/src/next.c L133-L147 -- C reference body.
 * Ref: src/ops/nexttoinf.ts  -- negative-x delegate (shipped, composite=1.0).
 * Ref: src/ops/nexttozero.ts -- non-negative-x delegate (shipped, composite=1.0).
 * Ref: src/core.ts -- locked MPFR shape, NAN_VALUE.
 */

import type { MPFR } from '../core.ts';
import { NAN_VALUE } from '../core.ts';
import { mpfr_nexttozero } from './nexttozero.ts';
import { mpfr_nexttoinf } from './nexttoinf.ts';

/**
 * Return the immediate predecessor of `x` in the direction of
 * `-infinity`, at x's own precision.
 *
 * @mpfrName mpfr_nextbelow
 *
 * @param x  The source {@link MPFR}. Any kind.
 *
 * @returns  A fresh `MPFR`. Specifically:
 *           - NaN     -> `NAN_VALUE`.
 *           - +/-0    -> -smallest representable at emin (sign normalised to -).
 *           - +/-Inf  -> -Inf unchanged / +Inf bumps to +largestFinite.
 *           - normal  -> one ULP toward `-inf` at the same precision.
 */
export function mpfr_nextbelow(x: MPFR): MPFR {
  // (1) NaN: propagate. The C sets a global flag we don't model.
  // Ref: mpfr/src/next.c L136-L140.
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }
  // (2) Sign-based dispatch -- inverted from nextabove. MPFR_IS_NEG reads
  // sign directly, so -0 takes the negative branch and nexttoinf
  // preserves the sign, landing on -smallest.
  // Ref: mpfr/src/next.c L142-L145.
  if (x.sign === -1) {
    return mpfr_nexttoinf(x);
  }
  return mpfr_nexttozero(x);
}
