/**
 * ops/free_pool.ts -- pure-TS port of MPFR's `mpfr_free_pool`.
 *
 * The C function (mpfr/src/pool.c L105-L118) iterates the static
 * `mpz_tab` pool, calls `mpz_clear` on each entry, and resets `n_alloc`
 * to 0. The pool exists to amortise mpz_init / mpz_clear churn across
 * calls in the C library.
 *
 * Algorithm (mpfr/src/pool.c L105-L118): walk the pool array, mpz_clear
 *   each, set n_alloc to 0.
 *
 * Ref: mpfr/src/pool.c L105-L118 -- C reference body.
 * Ref: mpfr/src/pool.c L48-L104  -- pool primitives this tears down.
 *
 * @divergence The TS port is a no-op. TypeScript has tracing GC; bigint
 *   values are not pool-allocated and there is no analog of `mpz_tab`.
 *   The function returns `true` as a success marker -- the wire-form
 *   codec does not natively express void/null scalar outputs, and the
 *   golden hardcodes `true` to match (consistent with
 *   `buildopt_*_p` no-op precedent).
 */

import type { MPFR as _MPFR } from '../core.ts';

/**
 * Free the (nonexistent) mpz pool.
 *
 * @mpfrName mpfr_free_pool
 *
 * @returns Always `true`. See `@divergence` above.
 */
export function mpfr_free_pool(): boolean {
  return true;
}
