/**
 * reference_ports/correct/mpfr_mpz_init.ts -- mutation-prove reference.
 *
 * The C function (mpfr/src/pool.c L36-L53) initialises an mpz_t to
 * value +0 (either by pool reclaim or fresh allocation). Per ADR 0003,
 * a bigint is the mpz_t analogue, and a freshly-initialised mpz_t with
 * SIZ=0 is just the bigint value 0n. There is no pool, no allocator;
 * the JS runtime handles bigint lifetimes.
 *
 * The TS port returns 0n unconditionally -- the vacuous factory analogue.
 */

// Law 4: every public port imports from the locked schema.
// MPFRError unused by the body (zero-arg factory, no input to validate)
// but keeps the AST-gate import surface satisfied.
import { MPFRError as _MPFRError } from '/home/tobiasosborne/Projects/mpfr-ts/src/core.ts';
void _MPFRError;

export function mpfr_mpz_init(): bigint {
  return 0n;
}
