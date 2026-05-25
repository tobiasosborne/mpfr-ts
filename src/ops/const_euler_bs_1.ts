/**
 * ops/const_euler_bs_1.ts -- pure-TS port of MPFR's
 * `mpfr_const_euler_bs_1`.
 *
 * Binary-splitting recursion for the first of two product series used in
 * the Euler-Mascheroni constant computation. The C function
 * (mpfr/src/const_euler.c L73-L135) walks the half-open integer range
 * `[n1, n2)` and produces six large integers `P, Q, T, C, D, V` satisfying
 * the binary-splitting recurrence for `S = sum_{n >= 1} (-1)^(n-1) N^n /
 * (n * n!)` plus the harmonic-sum cofactor used by
 * `mpfr_const_euler_internal`.
 *
 * Algorithm (faithful mirror of the C body)
 * -----------------------------------------
 *
 * Base case (`n2 - n1 == 1`):
 *
 *   let Nsq = N * N
 *   P = Nsq
 *   Q = (n1 + 1)^2
 *   T = Nsq             (= P)
 *   C = 1
 *   D = n1 + 1
 *   V = Nsq             (= P)
 *
 * Recursive case (`n2 - n1 > 1`):
 *
 *   m = floor((n1 + n2) / 2)
 *   L = bs1(n1, m, N, 1)
 *   R = bs1(m, n2, N, 1)
 *   P = if cont then L.P * R.P else 0
 *   Q = L.Q * R.Q
 *   D = L.D * R.D
 *   T = L.P * R.T + R.Q * L.T
 *   C = if cont then L.C * R.D + R.C * L.D else 0
 *   V = R.D * (R.Q * L.V + L.C * (L.P * R.T)) + L.D * L.P * R.V
 *
 * The `cont` flag controls whether the outer level recombines `P` and
 * `C`. At the top of the recursion `cont = 0` skips those two
 * combinations (the C code never assigns into `P` / `C` in that branch;
 * the underlying mpz_t inits to 0, so the TS port mirrors that with
 * literal `0n`). Recursive sub-calls always pass `cont = 1` because
 * the parent needs the full products for its own combination step.
 *
 * Sister to `mpfr_const_euler_bs_2`. The two BSes share the
 * harmonic-sum cofactor but otherwise carry different cofactor sets;
 * see `src/ops/const_euler_bs_2.ts` for the (P, Q, T)-only sibling.
 *
 * Storage: native BigInt, fresh record per call -- the C code uses
 * heap-allocated `mpz_t` with in-place mutation; the TS port is purely
 * functional, matching Law 3 (idiomatic immutable surface).
 *
 * Defensive validation: the C function is `static` and has no entry-
 * point checks. The TS port adds shallow type / range checks at the
 * public boundary (Rule 1) so misshapen inputs don't propagate silently
 * into the recursion.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/const_euler.c L73-L135 -- C reference body.
 *   - mpfr/src/const_euler.c L182-L275 -- parent
 *     `mpfr_const_euler_internal` (caller).
 *   - eval/functions/mpfr_const_euler_bs_1/spec.json -- contract.
 *   - src/ops/const_euler_bs_2.ts -- sister BS (P, Q, T only).
 *   - src/core.ts -- locked schema (type-only import for AST gate).
 */

import type { MPFR as _MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Six-tuple of bigints carried by the inner binary-splitting recursion.
 * Mirrors the (P, Q, T, C, D, V) mpz_t arguments mutated by the C
 * `mpfr_const_euler_bs_1`. Read-only at the function boundary.
 */
export interface Bs1Tuple {
  readonly P: bigint;
  readonly Q: bigint;
  readonly T: bigint;
  readonly C: bigint;
  readonly D: bigint;
  readonly V: bigint;
}

/**
 * Inner recursion -- skips entry-point validation. The caller (the
 * exported function below) has already verified the bigint shapes and
 * range invariants; recursive sub-calls reuse the validated inputs
 * directly.
 *
 * Direct mirror of mpfr/src/const_euler.c L73-L135.
 */
function bs1(n1: bigint, n2: bigint, N: bigint, cont: number): Bs1Tuple {
  if (n2 - n1 === 1n) {
    // Base case -- one term of the product series.
    // Ref: mpfr/src/const_euler.c L80-L91.
    const Nsq = N * N;
    const n1p1 = n1 + 1n;
    return {
      P: Nsq,
      Q: n1p1 * n1p1,
      T: Nsq,
      C: 1n,
      D: n1p1,
      V: Nsq,
    };
  }

  // Recursive case -- split the range and combine.
  // Ref: mpfr/src/const_euler.c L96 -- `m = (n1 + n2) / 2` (integer div).
  // BigInt `/` truncates toward zero; for non-negative n1, n2 this is the
  // same as `floor`. Per the C contract n1, n2 are `unsigned long`, so
  // non-negative is an invariant of the validated entry-point.
  const m = (n1 + n2) / 2n;
  // Inner recursive calls always pass cont=1 -- the parent needs the
  // full P / C factors to combine with the sibling at the next level up.
  const L = bs1(n1, m, N, 1);
  const R = bs1(m, n2, N, 1);

  // Ref: mpfr/src/const_euler.c L99-L121.
  //
  // When cont=0 the C code never writes into the mpz_t for P or C; the
  // underlying mpz_init left those at 0. We mirror that by binding 0n
  // directly. (T, Q, D, V are written unconditionally in both branches.)
  const P = cont !== 0 ? L.P * R.P : 0n;
  const Q = L.Q * R.Q;
  const D = L.D * R.D;

  // T = L.P * R.T + R.Q * L.T. Save the L.P * R.T product because the
  // V recurrence below reuses it (mirrors the C code's reuse of mpz `t`).
  const T_LR = L.P * R.T;
  const T = T_LR + R.Q * L.T;

  const C = cont !== 0 ? L.C * R.D + R.C * L.D : 0n;

  // V = R.D * (R.Q * L.V + L.C * L.P * R.T) + L.D * L.P * R.V.
  // Mirror the C reduction order (one big mpz that absorbs the
  // sub-products in place); native BigInt does not care, but matching
  // the structure makes line-by-line cross-reference trivial.
  const u = L.P * R.V * L.D;
  let v = R.Q * L.V + L.C * T_LR; // L.C * (L.P * R.T)
  v = v * R.D;
  const V = u + v;

  return { P, Q, T, C, D, V };
}

/**
 * Compute the (P, Q, T, C, D, V) sextuple produced by the
 * binary-splitting recursion over `[n1, n2)` for the Euler-Mascheroni
 * series.
 *
 * @mpfrName mpfr_const_euler_bs_1
 *
 * @param n1   Lower bound of the half-open range (inclusive). `>= 0`.
 * @param n2   Upper bound of the half-open range (exclusive). `> n1`.
 * @param N    Precision-derived constant; `>= 1`. Appears squared in
 *             the base-case P, T, V.
 * @param cont Combine-P/C flag at the outer level. `0` skips the
 *             `P *= L.P*R.P` and `C := L.C*R.D + R.C*L.D` steps in
 *             the recursive case (used at the top of the recursion to
 *             save multiplications); any nonzero value (typically `1`)
 *             performs the combine. Inner recursive calls always pass `1`.
 * @returns    `{P, Q, T, C, D, V}`. The shape matches the C function's
 *             six output mpz_t parameters.
 *
 * @throws {MPFRError} `EDOMAIN` if any of `n1`, `n2`, `N` is not a
 *         bigint, if `cont` is not an integer, if `n1 >= n2`, if
 *         `n1 < 0`, or if `N < 1`.
 *
 * @example
 *   mpfr_const_euler_bs_1(0n, 1n, 5n, 1);
 *   // => {P: 25n, Q: 1n, T: 25n, C: 1n, D: 1n, V: 25n}
 */
export function mpfr_const_euler_bs_1(
  n1: bigint,
  n2: bigint,
  N: bigint,
  cont: number,
): Bs1Tuple {
  if (
    typeof n1 !== 'bigint' ||
    typeof n2 !== 'bigint' ||
    typeof N !== 'bigint'
  ) {
    throw new MPFRError(
      'EDOMAIN',
      'mpfr_const_euler_bs_1: n1, n2, N must all be bigint',
    );
  }
  if (typeof cont !== 'number' || !Number.isInteger(cont)) {
    throw new MPFRError(
      'EDOMAIN',
      'mpfr_const_euler_bs_1: cont must be an integer (0 or nonzero)',
    );
  }
  if (n1 < 0n) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_const_euler_bs_1: requires n1 >= 0, got ${n1}`,
    );
  }
  if (n1 >= n2) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_const_euler_bs_1: requires n1 < n2, got n1=${n1}, n2=${n2}`,
    );
  }
  if (N < 1n) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_const_euler_bs_1: requires N >= 1, got ${N}`,
    );
  }
  return bs1(n1, n2, N, cont);
}
