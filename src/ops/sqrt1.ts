/**
 * ops/sqrt1.ts -- pure-TS port of MPFR's `mpfr_sqrt1`.
 *
 * Single-limb square-root fast path for prec(r) == prec(u) == p, where
 * 1 <= p < GMP_NUMB_BITS (i.e. 1 <= p <= 63 on a 64-bit GMP). The
 * dispatcher in mpfr_sqrt routes here at mpfr/src/sqrt.c L563-L564 when
 * those conditions hold (prec(r) == prec(u) AND prec < 64).
 *
 * C signature:
 *   static int mpfr_sqrt1(mpfr_ptr r, mpfr_srcptr u, mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port):
 *   mpfr_sqrt1(u, prec, rnd) -> Result
 *
 * Ref: mpfr/src/sqrt.c L74-L217 -- C reference body: exponent parity fix
 *   (L86-L92), __gmpfr_sqrt_limb_approx (L96), Newton fixup loop
 *   (L122-L131), rb/sb extraction (L137-L138), rounding dispatch
 *   (L179-L216).
 * Ref: mpfr/src/sqrt.c L561-L564 -- dispatcher condition:
 *   rq == uq AND rq < GMP_NUMB_BITS.
 *
 * Algorithm
 * ---------
 *
 * The C source uses __gmpfr_sqrt_limb_approx (mpfr/src/invsqrt_limb.h)
 * to produce a single-limb approximate root in [r0, r0+7], then a Newton
 * fixup loop to converge to the exact integer sqrt within that window, then
 * an explicit rb/sb extraction and five-mode rounding dispatch. All of this
 * is a *performance fast path*, not a distinct correctness contract: the
 * result is byte-identical to what the general mpfr_sqrt algorithm produces.
 *
 * The TS port delegates to src/ops/sqrt.ts (mpfr_sqrt), which uses bigint
 * isqrt and produces the same correctly-rounded output for every (u, prec,
 * rnd) triple. The C fast path is O(1) single-limb arithmetic; the TS path
 * is O(log prec) bigint Newton -- faster is a Production/Optimize concern,
 * not a correctness concern.
 *
 * Dispatcher precondition
 * -----------------------
 *
 * The C dispatcher (mpfr/src/sqrt.c L561-L564) requires:
 *   - prec(r) == prec(u)          (rq == uq)
 *   - prec(u) < GMP_NUMB_BITS     (rq < 64)
 *
 * This port enforces the same precondition, throwing MPFRError('EPREC', ...)
 * on violation. The golden driver only emits inputs that satisfy it; a
 * correct port never hits the throw branches on golden inputs.
 *
 * Divergence from C
 * -----------------
 *
 * 1. Return shape: C mutates `r` + returns int ternary. TS returns
 *    Result {value, ternary} per src/core.ts.
 * 2. Dispatcher precondition: enforced via MPFRError('EPREC', ...) rather
 *    than a C static assertion.
 * 3. Algorithm: bigint isqrt via mpfr_sqrt instead of __gmpfr_sqrt_limb_approx.
 * 4. Global flag side effects (mpfr_underflow/mpfr_overflow flag mutation)
 *    are omitted; the returned value is still correctly rounded.
 *
 * The pure-delegation body means mutate.py has no applicable mutation
 * sites; the gate fails for "no mutations applicable" rather than for
 * an undetected bug. Bd `mpfr-ts-9di` tracks the fix.
 *
 * Ref: eval/functions/mpfr_sqrt1/spec.json -- full divergence_from_c block.
 * Ref: docs/reports/010-shadow-trial.md -- shadow-trial precedent for this
 *   standalone-wire-form delegation pattern.
 * Ref: src/ops/div_2.ts -- structural precedent for the doc header style.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError } from '../core.ts';
import { mpfr_sqrt } from './sqrt.ts';

/** GMP_NUMB_BITS = 64. Ref: gmp.h / gmp-impl.h. */
const GMP_NUMB_BITS: bigint = 64n;

/**
 * Single-limb square-root fast path: prec(u) == prec AND prec < 64.
 *
 * Delegates to the unified mpfr_sqrt (which validates all arguments and
 * handles all singular cases), then enforces the dispatcher-specific
 * precondition (prec(u) == prec AND prec < GMP_NUMB_BITS).
 *
 * @mpfrName mpfr_sqrt1
 *
 * @param u    Operand. The C dispatcher routes only normal positive inputs
 *             here, but mpfr_sqrt (the delegate) handles all kinds correctly.
 * @param prec Target precision in bits. Must equal u.prec AND be < 64.
 * @param rnd  One of the five {@link RoundingMode} values.
 *
 * @returns {value, ternary}. Value passes validate(). Ternary is sign of
 *          (rounded - exact).
 *
 * @throws {MPFRError} EPREC if prec < 1 OR prec > PREC_MAX OR prec >= 64
 *         OR u.prec != prec (dispatcher precondition). EROUND on bad rnd.
 *
 * Ref: mpfr/src/sqrt.c L74-L217 -- C reference body.
 * Ref: mpfr/src/sqrt.c L561-L564 -- dispatcher routes here when
 *      prec(r) == prec(u) AND prec < GMP_NUMB_BITS (64).
 */
export function mpfr_sqrt1(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Dispatcher precondition: prec(u) == prec AND prec < GMP_NUMB_BITS.
  // Ref: mpfr/src/sqrt.c L561-L564 -- rq == uq AND rq < GMP_NUMB_BITS.
  if (u.prec !== prec) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sqrt1: u.prec (${u.prec}) must equal prec (${prec})`,
    );
  }
  if (prec >= GMP_NUMB_BITS) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sqrt1: prec (${prec}) must be < GMP_NUMB_BITS (${GMP_NUMB_BITS})`,
    );
  }
  // Ref: mpfr/src/sqrt.c L74-L217.
  return mpfr_sqrt(u, prec, rnd);
}
