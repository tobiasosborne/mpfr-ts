/**
 * ops/nexttoward.ts -- pure-TS port of MPFR's `mpfr_nexttoward`.
 *
 * Step x one ULP toward y at x's own precision; this is IEEE 754-1985
 * nextafter restricted to MPFR's precision lattice (NOT IEEE 754-2008
 * nextUp/nextDown, which is what nextabove/nextbelow implement).
 *
 * No rounding occurs (the "next" value is unique in the precision-defined
 * lattice), so the TS surface returns a fresh {@link MPFR} rather than a
 * `Result {value, ternary}` pair.
 *
 * C signature
 * -----------
 *
 *   void mpfr_nexttoward (mpfr_ptr x, mpfr_srcptr y);
 *
 * Body (mpfr/src/next.c L148-L172):
 *
 *   void mpfr_nexttoward (mpfr_ptr x, mpfr_srcptr y) {
 *     int s;
 *     if (MPFR_UNLIKELY(MPFR_IS_NAN(x))) {
 *       __gmpfr_flags |= MPFR_FLAGS_NAN;
 *       return;
 *     } else if (MPFR_UNLIKELY(MPFR_IS_NAN(x) || MPFR_IS_NAN(y))) {
 *       MPFR_SET_NAN(x);
 *       __gmpfr_flags |= MPFR_FLAGS_NAN;
 *       return;
 *     }
 *     s = mpfr_cmp(x, y);
 *     if (s == 0) return;
 *     else if (s < 0) mpfr_nextabove(x);
 *     else mpfr_nextbelow(x);
 *   }
 *
 * TS signature
 * ------------
 *
 *   mpfr_nexttoward(x: MPFR, y: MPFR): MPFR;
 *
 * Dispatch:
 *   - x is NaN -> NAN_VALUE.
 *   - y is NaN -> NAN_VALUE.
 *   - cmp(x, y) == 0 -> return x unchanged.
 *   - cmp(x, y) < 0 -> mpfr_nextabove(x).
 *   - cmp(x, y) > 0 -> mpfr_nextbelow(x).
 *
 * Divergence from C (spec.json)
 * -----------------------------
 *
 *   1. Return shape: C mutates x in place + returns void; TS returns a
 *      fresh MPFR. y is read-only on both sides.
 *
 *   2. NaN handling: C sets global __gmpfr_flags |= MPFR_FLAGS_NAN. TS
 *      has no global flag register; returns NAN_VALUE. Functional output
 *      (NaN in, NaN out) is identical.
 *
 *   3. NaN dispatch: TS port inspects kind === 'nan' BEFORE calling cmp
 *      (which throws on NaN per the idiomatic surface). C mpfr_cmp returns
 *      0 on NaN with erange flag; we don't surface erange.
 *
 * Signed-zero note: cmp(x, y) == 0 covers +/-0 vs +/-0 (mpfr_cmp returns
 * 0 for any pair of zeros regardless of sign). Returning x unchanged in
 * this case is the IEEE 754-1985 nextafter behaviour (as opposed to the
 * ISO C99 nextafter/nexttoward which return y).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/next.c L148-L172 -- C reference body.
 *   - mpfr/src/next.c L23-L41 -- IEEE 754-1985 vs IEEE 754-2008
 *     distinction comment.
 *   - src/ops/nextabove.ts -- delegate for cmp(x, y) < 0 case.
 *   - src/ops/nextbelow.ts -- delegate for cmp(x, y) > 0 case.
 *   - src/internal/mpfr/cmp_raw.ts -- non-throwing comparison core
 *     (returns null for NaN instead of throwing like mpfr_cmp).
 *   - src/core.ts -- locked schema, NAN_VALUE constant.
 *   - eval/functions/mpfr_nexttoward/spec.json -- divergence notes.
 *   - CLAUDE.md 'Hallucination-risk callouts' -- NaN propagation,
 *     signed zero observable.
 */

import type { MPFR } from '../core.ts';
import { NAN_VALUE } from '../core.ts';
import { compareMPFR } from '../internal/mpfr/cmp_raw.ts';
import { mpfr_nextabove } from './nextabove.ts';
import { mpfr_nextbelow } from './nextbelow.ts';

/**
 * Return the immediate successor of x in the direction of y, at x's own
 * precision. This is IEEE 754-1985 nextafter restricted to MPFR's
 * precision-defined lattice.
 *
 * @mpfrName mpfr_nexttoward
 *
 * @param x Source MPFR. Any kind.
 * @param y Target MPFR. Any kind.
 *
 * @returns A fresh MPFR. Specifically:
 *   - x or y is NaN -> NAN_VALUE.
 *   - cmp(x, y) == 0 -> x unchanged (including signed-zero distinction).
 *   - cmp(x, y) < 0 -> mpfr_nextabove(x) (step toward +inf).
 *   - cmp(x, y) > 0 -> mpfr_nextbelow(x) (step toward -inf).
 */
export function mpfr_nexttoward(x: MPFR, y: MPFR): MPFR {
  // (1) x is NaN: propagate. The C sets MPFR_FLAGS_NAN and returns;
  //     x remains NaN. TS has no global flag register.
  // Ref: mpfr/src/next.c L153-L157.
  if (x.kind === 'nan') {
    return NAN_VALUE;
  }

  // (2) y is NaN: set x to NaN (return NAN_VALUE). In the C source this
  //     is the else-if branch at L158: `MPFR_IS_NAN(x) || MPFR_IS_NAN(y)`
  //     where x is already known non-NaN (from (1)), so it activates only
  //     for y == NaN.
  // Ref: mpfr/src/next.c L158-L163.
  if (y.kind === 'nan') {
    return NAN_VALUE;
  }

  // (3) Compare x and y. We use compareMPFR (the non-throwing core from
  //     cmp_raw.ts) because the public mpfr_cmp throws EDOMAIN on NaN --
  //     we have already NaN-checked both operands above, so compareMPFR
  //     will never return null here.
  // Ref: mpfr/src/next.c L165.
  const s = compareMPFR(x, y);

  // Defensive guard: compareMPFR returns null for NaN, but both x and y
  // have been checked above. Should be unreachable.
  if (s === null) {
    return NAN_VALUE;
  }

  if (s === 0) {
    // x == y: return x unchanged. This is the load-bearing divergence
    // from ISO C99 nextafter/nexttoward (which return y). For signed
    // zeros, this means +0 stays +0 and -0 stays -0, preserving the
    // IEEE 754-1985 nextafter semantics even when signs differ.
    // Ref: mpfr/src/next.c L166-L167 (just `return;`).
    // Ref: mpfr/src/next.c L29-L33 (comment explaining the distinction).
    return x;
  }

  if (s < 0) {
    // x < y: step toward +infinity. Equivalent to mpfr_nextabove(x).
    // Ref: mpfr/src/next.c L168-L169.
    return mpfr_nextabove(x);
  }

  // s > 0: x > y, step toward -infinity. Equivalent to mpfr_nextbelow(x).
  // Ref: mpfr/src/next.c L170-L171.
  return mpfr_nextbelow(x);
}
