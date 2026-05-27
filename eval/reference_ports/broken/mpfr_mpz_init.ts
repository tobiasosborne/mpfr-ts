/**
 * reference_ports/broken/mpfr_mpz_init.ts -- deliberately-buggy.
 *
 * **Collapses output to 1n instead of 0n.** Every case fails on strict
 * equality (the only invariant of the function is that it returns 0n).
 */

export function mpfr_mpz_init(): bigint {
  return 1n;
}
