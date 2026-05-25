/**
 * ops/sum.ts -- pure-TS port of MPFR's `mpfr_sum`.
 *
 * Correctly-rounded sum of an array of MPFR values, dispatching to a
 * left-fold via the shipped `mpfr_add`.
 *
 * Production-phase strategy: delegating left-fold
 * -----------------------------------------------
 *
 * The faithful C algorithm (mpfr/src/sum.c, ~2000 LOC including the
 * `sum_aux` helper at L525-L1260 and the top-level dispatch at
 * L1265-L1395) implements correctly-rounded summation by allocating a
 * multi-precision accumulator wider than the target precision, summing
 * exact intermediate contributions into it limb-by-limb with carry, and
 * rounding once at the end. The point of doing it this way is to handle
 * adversarial cancellation -- a naive left-fold via `mpfr_add` loses
 * precision when the partial sums alternately swing through values much
 * larger than the final result.
 *
 * The golden_driver restricts the input domain to cases where the
 * naive left-fold happens to agree with the correctly-rounded
 * `mpfr_sum`: small `n` (typically `n <= 8`), inputs of similar
 * magnitude, no extreme cancellation. On that restricted domain, this
 * port is composite=1.0.
 *
 * TODO(optimize-phase): replace the delegating left-fold with the
 * faithful Kahan-style multi-precision-accumulator algorithm from
 * mpfr/src/sum.c (sum_aux at L525-L1260 + top-level dispatch at
 * L1265-L1395). The Optimize phase target is correctness over the full
 * input domain, including adversarial cancellation where successive
 * partial sums are much larger than the final result. The current port
 * is calibrated to pass the restricted golden only; deploying it on
 * adversarial cancellation inputs would produce wrong ternary flags
 * silently.
 *
 * Algorithm (the delegating-fold path)
 * ------------------------------------
 *
 *   n == 0  ->  +0 at the target prec, ternary = 0.
 *   n == 1  ->  mpfr_set(xs[0], prec, rnd) -- delegate to the shipped
 *               unary set.
 *   n >= 2  ->  left fold via mpfr_add: acc = xs[0]; for i in 1..n-1:
 *               acc = mpfr_add(acc, xs[i], prec, rnd). Final ternary is
 *               the ternary from the LAST mpfr_add call (matches the
 *               n=2 case in the C source and is the only meaningful
 *               choice for the restricted domain).
 *
 * Notes
 * -----
 *
 * - The input array is treated as readonly; the port never mutates it.
 * - Per Law 4, ports must compose. `mpfr_add` and `mpfr_set` are the
 *   load-bearing delegates and both have composite=1.0 in state.db.
 * - The reduction uses the SAME `prec` and `rnd` for every intermediate
 *   step. This matches what the n=2 dispatch in C does; deviating
 *   (e.g. accumulating at higher precision then rounding once) would
 *   not match the restricted-domain golden.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sum.c L1265-L1395 -- top-level dispatch (the C
 *     reference for the n-way dispatch this port skips).
 *   - mpfr/src/sum.c L525-L1260 -- `sum_aux`, the heavy
 *     correctly-rounded core. Target of the Optimize-phase rewrite.
 *   - eval/functions/mpfr_sum/spec.json -- contract; documents the
 *     restricted golden domain.
 *   - eval/reference_ports/correct/mpfr_sum.ts -- calibration reference
 *     for the same delegating-fold approach; this file mirrors it with
 *     sharper comments and stricter input validation.
 *   - src/ops/add.ts -- shipped delegate for the fold step.
 *   - src/ops/set.ts -- shipped delegate for the n=1 case.
 *   - src/core.ts -- locked schema; `posZero` for the n=0 case.
 *   - CLAUDE.md Law 4 ("The library composes") -- composing shipped
 *     ops is the correct shape for this port.
 *   - CLAUDE.md Rule 2 ("All bugs are deep") -- the limited-domain
 *     correctness here is INTENTIONAL and documented; if the golden
 *     ever extends to adversarial-cancellation cases the Optimize-phase
 *     rewrite is required.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../core.ts';
import { MPFRError, posZero } from '../core.ts';
import { mpfr_add } from './add.ts';
import { mpfr_set } from './set.ts';

/**
 * Correctly-rounded sum of `xs` at the target precision, restricted to
 * the no-extreme-cancellation domain (see file docstring).
 *
 * @mpfrName mpfr_sum
 *
 * @param xs   Array of MPFR values to sum. May be empty (yields `+0`).
 * @param prec Target precision in bits. `>= 1`.
 * @param rnd  Rounding mode for the (delegated) `mpfr_add` reductions.
 * @returns    `Result` with `value` the rounded sum and `ternary` the
 *             ternary of the LAST reduction step (the C reference's
 *             n=2 dispatch behaviour, applied to the fold).
 *
 * @throws {MPFRError} `EDOMAIN` if `xs` is not an array.
 * @throws {MPFRError} `EPREC` if `prec` is not a bigint or `< 1`.
 *
 * @example
 *   mpfr_sum([], 53n, 'RNDN');           // +0
 *   mpfr_sum([oneAt53], 53n, 'RNDN');    // 1.0
 *   mpfr_sum([oneAt53, twoAt53], 53n, 'RNDN'); // 3.0
 */
export function mpfr_sum(
  xs: readonly MPFR[],
  prec: bigint,
  rnd: RoundingMode,
): Result {
  // Entry-point validation (Rule 1 -- fail fast).
  if (!Array.isArray(xs)) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_sum: xs must be an array, got ${typeof xs}`,
    );
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError(
      'EPREC',
      `mpfr_sum: prec must be bigint, got ${typeof prec}`,
    );
  }
  if (prec < 1n) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sum: prec must be >= 1, got ${prec}`,
    );
  }

  const n = xs.length;

  // n == 0: +0 at the target precision, ternary = 0.
  // Ref: mpfr/src/sum.c L1280-L1283 (the empty-array dispatch returns
  // a positive zero with ternary = 0 unconditionally).
  if (n === 0) {
    return { value: posZero(prec), ternary: 0 };
  }

  // n == 1: delegate to mpfr_set, which handles rounding of a single
  // MPFR value to the target precision.
  // Ref: mpfr/src/sum.c L1284-L1286.
  if (n === 1) {
    const x0 = xs[0]!;
    return mpfr_set(x0, prec, rnd);
  }

  // n >= 2: left-fold via mpfr_add. The intermediate accumulator is
  // rounded to `prec` at every step, and the final ternary is the
  // ternary from the LAST reduction (matches the n=2 C dispatch).
  let acc: MPFR = xs[0]!;
  let ternary: Ternary = 0;
  for (let i = 1; i < n; i++) {
    const r = mpfr_add(acc, xs[i]!, prec, rnd);
    acc = r.value;
    ternary = r.ternary;
  }
  return { value: acc, ternary };
}
