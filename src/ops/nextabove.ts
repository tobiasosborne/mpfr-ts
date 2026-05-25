/**
 * ops/nextabove.ts -- pure-TS port of MPFR's `mpfr_nextabove`.
 *
 * Step `x` one ULP toward `+infinity` at x's own precision. This is the
 * IEEE 754-2008 `nextUp` operation restricted to a single MPFR precision.
 * No rounding occurs (the "next" value is unique in the precision-defined
 * lattice), so the TS surface returns a fresh {@link MPFR} rather than a
 * `Result {value, ternary}` pair.
 *
 * The 11-line C body is pure sign-dispatch over two already-ported
 * helpers, so this file is a thin delegator:
 *
 *   - NaN          -> {@link NAN_VALUE} (C sets MPFR_FLAGS_NAN; not modelled).
 *   - sign === -1  -> {@link mpfr_nexttozero} (stepping above a negative
 *                     value moves it closer to zero, i.e. less negative).
 *   - otherwise    -> {@link mpfr_nexttoinf}  (stepping above non-negative
 *                     moves toward +inf).
 *
 * Signed-zero asymmetry: nextabove(+0) routes via nexttoinf and yields
 * +smallest (sign preserved); nextabove(-0) routes via nexttozero and
 * yields +smallest (sign flipped inside nexttozero). Both directions
 * land on +smallest, matching IEEE 754 `nextUp(+/-0)`.
 *
 * Ref: mpfr/src/next.c L119-L131 -- C reference body.
 * Ref: src/ops/nexttozero.ts -- negative-x delegate (shipped, composite=1.0).
 * Ref: src/ops/nexttoinf.ts  -- non-negative-x delegate (shipped, composite=1.0).
 * Ref: src/core.ts -- locked MPFR shape, NAN_VALUE.
 */

import type { MPFR } from '../core.ts';
import { NAN_VALUE } from '../core.ts';
import { mpfr_nexttozero } from './nexttozero.ts';
import { mpfr_nexttoinf } from './nexttoinf.ts';

/**
 * Return the immediate successor of `x` in the direction of `+infinity`,
 * at x's own precision.
 *
 * @mpfrName mpfr_nextabove
 *
 * @param x  The source {@link MPFR}. Any kind.
 *
 * @returns  A fresh `MPFR`. Specifically:
 *           - NaN     -> `NAN_VALUE`.
 *           - +/-0    -> +smallest representable at emin (sign normalised to +).
 *           - +/-Inf  -> +Inf unchanged / -Inf bumps to -largestFinite.
 *           - normal  -> one ULP toward `+inf` at the same precision.
 */
export function mpfr_nextabove(x: MPFR): MPFR {
  // (1) NaN: propagate. The C sets a global flag we don't model.
  // Ref: mpfr/src/next.c L122-L126.
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }
  // (2) Sign-based dispatch. MPFR_IS_NEG reads sign directly, so -0 takes
  // the negative branch (its nexttozero sign-flips back to +smallest).
  // Ref: mpfr/src/next.c L127-L130.
  if (x.sign === -1) {
    return mpfr_nexttozero(x);
  }
  return mpfr_nexttoinf(x);
}
