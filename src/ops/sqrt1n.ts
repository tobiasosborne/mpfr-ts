/**
 * ops/sqrt1n.ts -- pure-TS port of MPFR's `mpfr_sqrt1n`.
 *
 * Single-limb sqrt fast path: prec(r) == GMP_NUMB_BITS (64) and
 * prec(u) <= GMP_NUMB_BITS. The dispatcher in mpfr_sqrt routes here
 * at mpfr/src/sqrt.c L569-L570 when those conditions hold.
 *
 * C signature:
 *   static int mpfr_sqrt1n(mpfr_ptr r, mpfr_srcptr u, mpfr_rnd_t rnd_mode)
 *
 * TS signature (this port):
 *   mpfr_sqrt1n(u: MPFR, prec: bigint, rnd: RoundingMode): Result
 *
 * Ref: mpfr/src/sqrt.c L220-L346 -- the C reference body.
 * Ref: mpfr/src/sqrt.c L569-L570 -- dispatcher routes here when
 *      prec(r) == GMP_NUMB_BITS.
 * Ref: mpfr/src/sqrt.c L229 -- asserts prec(u) <= GMP_NUMB_BITS.
 *
 * Algorithm
 * ---------
 *
 * The C source uses __gmpfr_sqrt_limb_approx (an integer-sqrt
 * approximation within [r0, r0+7]) plus a Newton fixup loop
 * (L256-L265) and then rounds with one of the five MPFR rounding
 * modes. The TS port delegates to src/ops/sqrt.ts's unified
 * mpfr_sqrt which produces byte-identical output via bigint isqrt.
 *
 * Entry conditions (dispatcher enforces; this port asserts at entry):
 *   - prec === 64n
 *   - u.prec >= 1n && u.prec <= 64n
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError, PREC_MIN } from '../core.ts';
import { mpfr_sqrt } from './sqrt.ts';

/** GMP_NUMB_BITS. Ref: gmp.h. */
const GMP_NUMB_BITS: bigint = 64n;

/**
 * Fast-path sqrt for prec(r) == 64 and prec(u) <= 64.
 *
 * Enforces the dispatcher precondition at entry; delegates to the
 * unified mpfr_sqrt for correctness.
 *
 * @mpfrName mpfr_sqrt1n
 *
 * @param u    Operand -- must satisfy u.prec in [1, 64].
 * @param prec Target precision -- must be exactly 64n.
 * @param rnd  Rounding mode.
 *
 * @returns {value, ternary} per src/core.ts.
 *
 * @throws {MPFRError} EPREC if prec !== 64n or u.prec > 64n.
 *
 * Ref: mpfr/src/sqrt.c L220-L346 -- C reference body.
 * Ref: mpfr/src/sqrt.c L228-L229 -- dispatcher precondition asserts.
 */
export function mpfr_sqrt1n(
  u: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Enforce dispatcher precondition: prec(r) == GMP_NUMB_BITS.
  // Ref: mpfr/src/sqrt.c L228 -- MPFR_ASSERTD(MPFR_PREC(r) == GMP_NUMB_BITS).
  if (prec !== GMP_NUMB_BITS) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sqrt1n: prec must be ${GMP_NUMB_BITS}, got ${prec}`,
    );
  }

  // Enforce dispatcher precondition: prec(u) <= GMP_NUMB_BITS.
  // Ref: mpfr/src/sqrt.c L229 -- MPFR_ASSERTD(MPFR_PREC(u) <= GMP_NUMB_BITS).
  if (u.prec < PREC_MIN || u.prec > GMP_NUMB_BITS) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sqrt1n: u.prec must be in [1, ${GMP_NUMB_BITS}], got ${u.prec}`,
    );
  }

  // Delegate to the unified sqrt -- same correctness contract, same
  // edge cases, byte-identical output for all (u, prec=64, rnd) triples.
  // Ref: spec.json divergence_from_c #5 -- algorithm divergence, same output.
  return mpfr_sqrt(u, prec, rnd);
}
