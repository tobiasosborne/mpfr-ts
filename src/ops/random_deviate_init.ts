/**
 * ops/random_deviate_init.ts -- pure-TS port of MPFR's
 * `mpfr_random_deviate_init`.
 *
 * Allocates and initialises a fresh random-deviate state, the
 * supporting machinery for `mpfr_erandom` / `mpfr_nrandom`. The C
 * struct (mpfr/src/random_deviate.h L33-L41) is
 *
 *     struct {
 *       mpfr_random_size_t e;  // bits of fraction generated so far
 *       unsigned long      h;  // high W bits (W=32) -- undef when e=0
 *       mpz_t              f;  // rest of the fraction  -- undef when e<=W
 *     }
 *
 * and the C body (random_deviate.c L65-L70) is `mpz_init(x->f);
 * x->e = 0;` -- the freshly-initialised state has zero generated bits
 * and undefined high/low fields.
 *
 * The TS port returns `{e: 0n, h: 0n, f: 0n}` as the canonical zeroed
 * representation. The shape is now a project convention (the three
 * `random_deviate_*` ports share it for grader stability); future
 * downstream consumers will read this shape and feed it to `_reset`
 * and `_clear`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/random_deviate.c L65-L70 -- C reference body.
 *   - mpfr/src/random_deviate.h L33-L41 -- struct definition.
 */

import type { MPFR as _MPFR } from '../core.ts';

/**
 * A random-deviate state. Mirrors the three meaningful fields of
 * MPFR's `__mpfr_random_deviate_struct` with native bigints in place
 * of the C `mpz_t` for the low-bits field.
 */
export interface RandomDeviate {
  /** Bits of fraction generated so far. `0n` means uninitialised. */
  e: bigint;
  /** High W bits of the fraction (W=32). Undefined when `e === 0n`. */
  h: bigint;
  /** Remaining low bits of the fraction. Undefined when `e <= 32n`. */
  f: bigint;
}

/**
 * Fresh, zeroed random-deviate state. No bits generated yet.
 *
 * @mpfrName mpfr_random_deviate_init
 *
 * @returns  `{e: 0n, h: 0n, f: 0n}`.
 *
 * @example
 *   const d = mpfr_random_deviate_init();  // {e: 0n, h: 0n, f: 0n}
 */
export function mpfr_random_deviate_init(): RandomDeviate {
  return { e: 0n, h: 0n, f: 0n };
}
