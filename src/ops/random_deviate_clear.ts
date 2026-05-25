/**
 * ops/random_deviate_clear.ts -- pure-TS port of MPFR's
 * `mpfr_random_deviate_clear`.
 *
 * The C body is `mpz_clear(x->f)` -- frees the heap-allocated GMP
 * integer holding the low fraction bits. There is no analogue in the
 * TS surface: bigints are GC-managed and there is nothing to free.
 *
 * The cleanest immutable representation of "this state is no longer
 * in use" is to return a fully-zeroed state -- the same shape that
 * `mpfr_random_deviate_init` produces. The input is ignored.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/random_deviate.c L80-L84 -- C reference body.
 *   - mpfr/src/random_deviate.h L33-L41 -- struct definition.
 *   - eval/functions/mpfr_random_deviate_init/spec.json -- sister
 *     function (same return shape).
 */

import type { MPFR as _MPFR } from '../core.ts';

/** See `mpfr_random_deviate_init` for the field contract. */
export interface RandomDeviate {
  e: bigint;
  h: bigint;
  f: bigint;
}

/**
 * Release a random-deviate state. In the GC'd TS surface this is a
 * no-op semantically; the returned zeroed state is the immutable
 * analog to "freed".
 *
 * @mpfrName mpfr_random_deviate_clear
 *
 * @param _x  Deviate state to release. Ignored; the output is always
 *            the zeroed state.
 * @returns   `{e: 0n, h: 0n, f: 0n}`.
 *
 * @example
 *   mpfr_random_deviate_clear({e: 42n, h: 7n, f: 11n});
 *   // {e: 0n, h: 0n, f: 0n}
 */
export function mpfr_random_deviate_clear(_x: RandomDeviate): RandomDeviate {
  return { e: 0n, h: 0n, f: 0n };
}
