/**
 * ops/const_euler_bs_clear.ts -- pure-TS port of MPFR's
 * `mpfr_const_euler_bs_clear`.
 *
 * Internal helper used by the Euler-Mascheroni constant computation. The C
 * side calls `mpz_clear` on each of the 6 mpz_t fields of a
 * `mpfr_const_euler_bs_struct` (mpfr/src/const_euler.c L39-L49), freeing
 * the heap-allocated GMP integers.
 *
 * BigInt in TS is garbage-collected -- there is nothing to free. The
 * closest immutable analog to "cleared" is returning a state with all
 * six fields reset to zero. This matches the golden, which records the
 * post-clear shape (every field `"0"`).
 *
 * The input `s` is intentionally unused -- the C side's job is destructor
 * cleanup, and the TS analog is constructing the zeroed state independent
 * of what was in the input. The parameter is kept in the signature for
 * symmetry with the C contract and to let the runner verify the input
 * decodes cleanly through the struct path.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/const_euler.c L62-L71 -- C reference body.
 *   - mpfr/src/const_euler.c L39-L49 -- struct definition.
 *   - eval/functions/mpfr_const_euler_bs_clear/spec.json -- contract.
 *   - src/ops/const_euler_bs_init.ts -- sister constructor.
 *   - src/core.ts -- locked schema (type-only import for AST gate).
 */

import type { MPFR as _MPFR } from '../core.ts';
import type { ConstEulerBs } from './const_euler_bs_init.ts';

export type { ConstEulerBs };

/**
 * "Free" an Euler binary-splitting state. Returns the zero state -- a
 * GC-correct analog to the C `mpz_clear`-on-each-field destructor.
 *
 * @mpfrName mpfr_const_euler_bs_clear
 *
 * @param _s The state to clear. Unused -- BigInt is GC-managed -- but
 *           accepted for signature symmetry with the C contract.
 * @returns The zero state `{P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n}`.
 *
 * @example
 *   const s = mpfr_const_euler_bs_init();
 *   mpfr_const_euler_bs_clear(s);
 *   // => {P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n}
 */
export function mpfr_const_euler_bs_clear(_s: ConstEulerBs): ConstEulerBs {
  // _s is GC-managed; no heap to release. Returning a fresh zero state
  // mirrors the post-clear shape the golden records.
  return { P: 0n, Q: 0n, T: 0n, C: 0n, D: 0n, V: 0n };
}
