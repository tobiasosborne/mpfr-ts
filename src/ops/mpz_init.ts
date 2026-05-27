/**
 * ops/mpfr_mpz_init.ts -- pure-TS port of MPFR's `mpfr_mpz_init`.
 *
 * Misc-class vacuous factory. In C (mpfr/src/pool.c L36-L53), the function
 * either reclaims an mpz_t from the static mpz_tab pool or calls the real
 * mpz_init to allocate fresh limb storage; in both cases SIZ(z) is set to 0
 * so the value is +0.
 *
 * The TS port is a vacuous factory returning 0n -- the analogue of a
 * freshly-initialised mpz_t whose value is 0. Per ADR 0003 (bigint-as-mpz),
 * there is no allocator, no pool, no explicit init: a bigint is a JS-runtime
 * managed primitive that exists from the moment of assignment. The function
 * is preserved at the public surface because (a) the C side has the symbol,
 * (b) downstream MPFR callers may transitively reference it via the
 * bigint-as-mpz wrapper layer in future ports, and (c) the round-trip count
 * is part of the harness's vacuity verification.
 *
 * Returning 0n matches the conventional shape of an init-without-value
 * primitive: in MPFR, mpfr_init2 likewise produces a NaN-or-zero placeholder;
 * in the bigint world, an uninitialised integer is simply 0n.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/pool.c L36-L53 -- the C reference body.
 *   - eval/functions/mpfr_mpz_clear/spec.json -- the inverse (vacuous pool
 *     teardown).
 *   - eval/functions/mpfr_free_pool/spec.json -- precedent (no-arg vacuous
 *     accessor returning null).
 *   - docs/adr/0003-mpz-api.md -- bigint-as-mpz_t; no pool, no explicit init.
 *   - HANDOFF.md gotcha #9 (worklog 020) -- count-passthrough / scalar-marker
 *     for void-returning ops.
 *
 * @returns Always `0n` -- the value of a freshly-initialised mpz_t.
 */
import type { MPFR } from "../core.ts";

export function mpfr_mpz_init(): bigint {
  return 0n;
}
