/**
 * ops/roundeven.ts -- pure-TS port of MPFR's `mpfr_roundeven`.
 *
 * Round an MPFR value to the nearest prec-representable integer, ties to
 * EVEN (RNDN semantics). No rnd parameter -- the direction is implicit and
 * fixed to ties-to-even.
 *
 * C body is a one-line delegation to mpfr_rint:
 *
 *   int mpfr_roundeven(mpfr_ptr r, mpfr_srcptr u)
 *   { return mpfr_rint(r, u, MPFR_RNDN); }
 *
 * Ref: mpfr/src/rint.c L308-L312 -- wrapper.
 * Ref: mpfr/src/rint.c L35-L304 -- mpfr_rint engine.
 *
 * TS signature divergence: the port takes prec as a positional argument
 * (the C function carries prec on the rop handle) and returns `{value, ternary}`
 * per the locked Result schema. Ternary is normalized to {-1,0,1}.
 *
 * @mpfrName mpfr_roundeven
 *
 * @param x    operand (any kind: normal, zero, inf, nan).
 * @param prec output precision in **bits**, in `[PREC_MIN, PREC_MAX]`.
 *
 * @returns `{value, ternary}` -- the rounded value and ternary flag.
 *
 * @throws {MPFRError} `EPREC` on bad precision.
 */

import type { MPFR, Result } from "../core.ts";
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from "../core.ts";
import { mpfr_rint } from "./rint.ts";

export function mpfr_roundeven(x: MPFR, prec: bigint): Result {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  return mpfr_rint(x, prec, 'RNDN');
}
