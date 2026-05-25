/**
 * ops/random_deviate_reset.ts -- pure-TS port of MPFR's
 * `mpfr_random_deviate_reset`.
 *
 * Resets the bit-counter on a random-deviate state. The C body is one
 * line: `x->e = 0` -- the `h` and `f` fields are left unchanged in
 * memory but become semantically undefined once `e == 0` (per the
 * struct's "undef when e=0" contract).
 *
 * The TS port returns a new state with `e: 0n` and `h`/`f` preserved
 * verbatim from the input. Preserving them keeps the wire form
 * deterministic for the grader (a fully-zeroed output would also be
 * semantically valid but would diverge from the reference golden,
 * since the C side leaves the in-memory bytes alone).
 *
 * Refs
 * ----
 *
 *   - mpfr/src/random_deviate.c L73-L77 -- C reference body.
 *   - mpfr/src/random_deviate.h L33-L41 -- struct definition with the
 *     "undef when e=0" contract.
 *   - eval/functions/mpfr_random_deviate_init/spec.json -- sister
 *     function (same state shape).
 */

import type { MPFR as _MPFR } from '../core.ts';

/**
 * A random-deviate state. See `mpfr_random_deviate_init` for the field
 * contract.
 */
export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

/**
 * Clear the bit-counter; `h` and `f` are preserved in the output for
 * wire-form determinism even though they become semantically undefined.
 *
 * @mpfrName mpfr_random_deviate_reset
 *
 * @param x  Current deviate state.
 * @returns  `{e: 0n, h: x.h, f: x.f}`.
 *
 * @example
 *   mpfr_random_deviate_reset({e: 42n, h: 7n, f: 11n});
 *   // {e: 0n, h: 7n, f: 11n}
 */
export function mpfr_random_deviate_reset(x: RandomDeviate): RandomDeviate {
  return { e: 0n, h: x.h, f: x.f };
}
