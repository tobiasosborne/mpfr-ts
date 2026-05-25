/**
 * ops/buildopt_tls_p.ts -- pure-TS port of MPFR's `mpfr_buildopt_tls_p`.
 *
 * Build-time predicate: was this libmpfr compiled with thread-local-storage
 * support (`MPFR_USE_THREAD_SAFE`)?
 *
 * The C reference is a one-line preprocessor branch returning 0 or 1. The
 * TS port returns the compile-time constant `false`: pure-TS modules run
 * single-threaded under a JS event loop, and each Worker is its own isolate
 * with module-local state by construction. There is no shared mutable
 * `__gmpfr_flags` register across threads in mpfr-ts, so the "does this
 * build have TLS" question has no meaningful TS analogue -- returning
 * `false` is the only honest answer and matches the rest of the
 * `buildopt_*_p` family.
 *
 * The type-only `core.ts` import satisfies the AST gate (Law 4) without
 * touching the runtime value model.
 *
 * Ref: mpfr/src/buildopt.c L25-L33 -- C reference (preprocessor branch on
 *   MPFR_USE_THREAD_SAFE).
 * Ref: src/ops/buildopt_sharedcache_p.ts -- sibling no-arg predicate.
 */

import type { MPFR as _MPFR } from '../core.ts';

/**
 * Predicate: does this build expose thread-local storage for the global
 * MPFR flag register?
 *
 * @mpfrName mpfr_buildopt_tls_p
 *
 * @returns Always `false` in the pure-TS port. Workers are isolates with
 *          module-local state; no shared mutable register exists, so TLS
 *          has no meaningful TS analogue.
 *
 * @example
 *   mpfr_buildopt_tls_p();  // false
 */
export function mpfr_buildopt_tls_p(): boolean {
  // Ref: mpfr/src/buildopt.c L25-L33 -- preprocessor branch on
  // MPFR_USE_THREAD_SAFE. No shared cross-Worker state in mpfr-ts.
  return false;
}
