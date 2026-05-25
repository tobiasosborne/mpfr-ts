/**
 * ops/const_euler_bs_2.ts -- pure-TS port of MPFR's
 * `mpfr_const_euler_bs_2`.
 *
 * Binary-splitting recursion for the second of two product series used in
 * the Euler-Mascheroni constant computation. The C function (mpfr/src/
 * const_euler.c L138-L180) walks the half-open integer range `[n1, n2)`
 * and produces three large integers `P`, `Q`, `T` satisfying the
 * binary-splitting recurrence used by `mpfr_const_euler_internal`.
 *
 * Algorithm (faithful mirror of the C body)
 * -----------------------------------------
 *
 * Base case (`n2 - n1 == 1`):
 *
 *   if n1 == 0:
 *     P = 1
 *     Q = 4 * N
 *   else:
 *     P = (2*n1 - 1)^3
 *     Q = 32 * n1 * N^2
 *   T = P
 *
 * Recursive case (`n2 - n1 > 1`):
 *
 *   m = floor((n1 + n2) / 2)
 *   (P,  Q,  T)  = bs2(n1, m, N, cont=1)
 *   (P2, Q2, T2) = bs2(m, n2, N, cont=1)
 *   T = T * Q2 + T2 * P
 *   if cont: P = P * P2
 *   Q = Q * Q2
 *
 * The `cont` flag controls whether the outer level recombines P. At the
 * top of the recursion `cont = 0` saves one multiplication; recursive
 * sub-calls always pass `cont = 1` because the parent needs the full
 * product for its own combination step.
 *
 * Storage: the C code uses heap-allocated `mpz_t` GMP integers and
 * mutates them in place. The TS port uses native BigInt arithmetic and
 * returns a fresh `{P, Q, T}` record per call -- the recursion is then
 * purely functional, matching Law 3 (idiomatic immutable surface).
 *
 * Defensive validation: the C function is `static` and has no entry-point
 * checks. The TS port adds shallow type / range checks at the public
 * boundary (Rule 1 -- fail fast on invariant violations) so that
 * misshapen bigints don't propagate silently into the recursion. The
 * inner recursion skips these checks for efficiency.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/const_euler.c L138-L180 -- C reference body.
 *   - mpfr/src/const_euler.c L182-L275 -- parent
 *     `mpfr_const_euler_internal` (caller).
 *   - eval/functions/mpfr_const_euler_bs_2/spec.json -- contract.
 *   - src/ops/const_euler_bs_init.ts -- sister state-allocator (the
 *     parent state is a different shape: `ConstEulerBs` carries C/D/V too).
 *   - src/core.ts -- locked schema (type-only import for AST gate).
 */

import type { MPFR as _MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Three-tuple of bigints carried by the inner binary-splitting recursion.
 * Mirrors the (P, Q, T) trio of mpz_t arguments mutated by the C
 * `mpfr_const_euler_bs_2`.
 */
export interface BsTriple {
  readonly P: bigint;
  readonly Q: bigint;
  readonly T: bigint;
}

/**
 * Inner recursion -- skips entry-point validation. The caller (the
 * exported function below) has already verified the bigint shapes and
 * range invariants; the recursive sub-calls reuse the validated inputs
 * directly.
 *
 * Direct mirror of mpfr/src/const_euler.c L138-L180.
 */
function bs2(n1: bigint, n2: bigint, N: bigint, cont: number): BsTriple {
  if (n2 - n1 === 1n) {
    // Base case -- one term of the product series.
    let P: bigint;
    let Q: bigint;
    if (n1 === 0n) {
      // Ref: mpfr/src/const_euler.c L146-L149.
      P = 1n;
      Q = 4n * N;
    } else {
      // Ref: mpfr/src/const_euler.c L150-L157. (2*n1 - 1)^3 and 32*n1*N^2.
      const twoN1m1 = 2n * n1 - 1n;
      P = twoN1m1 * twoN1m1 * twoN1m1;
      Q = 32n * n1 * N * N;
    }
    // Ref: mpfr/src/const_euler.c L158 -- mpz_set (T, P).
    const T = P;
    return { P, Q, T };
  }

  // Recursive case -- split the range and combine.
  // Ref: mpfr/src/const_euler.c L163 -- `m = (n1 + n2) / 2` (integer div).
  // BigInt `/` truncates toward zero; for non-negative n1, n2 this is the
  // same as `floor`. Per the C contract n1, n2 are `unsigned long`, so
  // non-negative is an invariant.
  const m = (n1 + n2) / 2n;
  // Inner recursive calls always pass cont=1 -- the parent needs the
  // full P factor to combine with the sibling at the next level up.
  const left = bs2(n1, m, N, 1);
  const right = bs2(m, n2, N, 1);
  // Ref: mpfr/src/const_euler.c L170-L172 -- T = T * Q2 + T2 * P. The
  // multiplications use the LEFT side's P (the original `P`), not the
  // freshly combined product, so we read `left.P` here rather than the
  // post-combine value.
  const T = left.T * right.Q + right.T * left.P;
  // Ref: mpfr/src/const_euler.c L173-L174 -- P combined only when cont
  // is set. At the top of the recursion `cont = 0` saves a multiplication
  // because the caller does not need the combined P.
  const P = cont !== 0 ? left.P * right.P : left.P;
  // Ref: mpfr/src/const_euler.c L175 -- Q always combined.
  const Q = left.Q * right.Q;
  return { P, Q, T };
}

/**
 * Compute the (P, Q, T) triple produced by the binary-splitting recursion
 * over `[n1, n2)` for the Euler-Mascheroni series.
 *
 * @mpfrName mpfr_const_euler_bs_2
 *
 * @param n1   Lower bound of the half-open range (inclusive). `>= 0`.
 * @param n2   Upper bound of the half-open range (exclusive). `> n1`.
 * @param N    Precision-derived constant; `>= 1`. Appears in `Q = 4*N`
 *             (base case `n1 = 0`) and `Q = 32*n1*N^2` (other base case).
 * @param cont Combine-P flag at the outer level. `0` skips the
 *             `P *= P2` step (used at the top of the recursion to save a
 *             multiplication); any nonzero value (typically `1`) performs
 *             the combine. Inner recursive calls always pass `1`.
 * @returns    `{P, Q, T}`. The shape matches the C function's three
 *             output mpz_t parameters.
 *
 * @throws {MPFRError} `EDOMAIN` if any of `n1`, `n2`, `N` is not a
 *         bigint, if `cont` is not an integer, if `n1 >= n2`, or if
 *         `N < 1`.
 *
 * @example
 *   mpfr_const_euler_bs_2(0n, 1n, 5n, 1);
 *   // => {P: 1n, Q: 20n, T: 1n}
 *   mpfr_const_euler_bs_2(0n, 2n, 5n, 1);
 *   // => {P: 1n, Q: 16000n, T: 801n}
 */
export function mpfr_const_euler_bs_2(
  n1: bigint,
  n2: bigint,
  N: bigint,
  cont: number,
): BsTriple {
  if (typeof n1 !== 'bigint' || typeof n2 !== 'bigint' || typeof N !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      'mpfr_const_euler_bs_2: n1, n2, N must all be bigint',
    );
  }
  if (typeof cont !== 'number' || !Number.isInteger(cont)) {
    throw new MPFRError(
      'EDOMAIN',
      'mpfr_const_euler_bs_2: cont must be an integer (0 or nonzero)',
    );
  }
  if (n1 < 0n) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_const_euler_bs_2: requires n1 >= 0, got ${n1}`,
    );
  }
  if (n1 >= n2) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_const_euler_bs_2: requires n1 < n2, got n1=${n1}, n2=${n2}`,
    );
  }
  if (N < 1n) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_const_euler_bs_2: requires N >= 1, got ${N}`,
    );
  }
  return bs2(n1, n2, N, cont);
}
