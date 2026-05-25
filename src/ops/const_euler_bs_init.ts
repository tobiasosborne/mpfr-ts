/**
 * ops/const_euler_bs_init.ts -- pure-TS port of MPFR's
 * `mpfr_const_euler_bs_init`.
 *
 * Internal helper used by the Euler-Mascheroni constant computation. The C
 * side calls `mpz_init` on each of the 6 mpz_t fields of a
 * `mpfr_const_euler_bs_struct` (mpfr/src/const_euler.c L39-L49):
 *
 *     struct mpfr_const_euler_bs_struct {
 *       mpz_t P; mpz_t Q; mpz_t T;
 *       mpz_t C; mpz_t D; mpz_t V;
 *     };
 *
 * Each `mpz_init` allocates a fresh GMP integer initialised to zero.
 *
 * The TS port returns a fresh object with six zero bigints. Native BigInt
 * has no separate "init" step -- the literal `0n` IS a zero-valued
 * arbitrary-precision integer. Returning a fresh immutable record matches
 * the idiomatic-surface contract (Law 3) while preserving the C side's
 * "produces six zero fields" semantics.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/const_euler.c L51-L60 -- C reference body.
 *   - mpfr/src/const_euler.c L39-L49 -- struct definition.
 *   - eval/functions/mpfr_const_euler_bs_init/spec.json -- contract.
 *   - src/ops/const_euler_bs_clear.ts -- sister destructor.
 *   - src/ops/const_euler_bs_2.ts -- recursion that consumes/produces
 *     a related triple-shaped state.
 *   - src/core.ts -- locked schema (type-only import for AST gate).
 */

import type { MPFR as _MPFR } from '../core.ts';

/**
 * 6-tuple of bigints carried by the Euler-Mascheroni binary-splitting
 * recursion. Mirrors `mpfr_const_euler_bs_t` (mpfr/src/const_euler.c
 * L39-L49). Immutable -- ops that "mutate" the state in C return a
 * fresh record in the TS port.
 */
export interface ConstEulerBs {
  readonly P: bigint;
  readonly Q: bigint;
  readonly T: bigint;
  readonly C: bigint;
  readonly D: bigint;
  readonly V: bigint;
}

/**
 * Construct a freshly-initialised Euler binary-splitting state. Every
 * field is zero -- the equivalent of calling `mpz_init` on each of the
 * six mpz_t fields of the C struct.
 *
 * @mpfrName mpfr_const_euler_bs_init
 *
 * @returns The zero state `{P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n}`.
 *
 * @example
 *   mpfr_const_euler_bs_init();
 *   // => {P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n}
 */
export function mpfr_const_euler_bs_init(): ConstEulerBs {
  return { P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n };
}
