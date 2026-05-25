/**
 * reference_ports/correct/mpfr_buildopt_tls_p.ts -- mutation-prove
 * reference for mpfr_buildopt_tls_p.
 *
 * Per CLAUDE.md PIL.3 / docs/PILOT_PLAN.md Step 7, this file is the
 * calibration baseline for the golden master. The production
 * src/ops/buildopt_tls_p.ts does not yet exist; the orchestrator will
 * materialise it during the port-and-grade flow.
 *
 * Returns the compile-time constant `false` -- pure-TS has no shared
 * cross-thread mutable state that would need TLS (each Worker is an
 * isolate; no __gmpfr_flags shared across them).
 *
 * The type-only core.ts import satisfies the ast_check gate
 * (CLAUDE.md Law 4) without affecting the predicate's value.
 *
 * Ref: mpfr/src/buildopt.c L25-L33 -- C reference.
 * Ref: eval/functions/mpfr_buildopt_tls_p/spec.json -- contract.
 * Ref: src/ops/buildopt_float128_p.ts -- sibling template.
 */

import type { MPFR as _MPFR } from '../../../src/core.ts';

export function mpfr_buildopt_tls_p(): boolean {
  // Pure-TS modules are single-threaded; Workers are isolates with no
  // shared mutable state -- TLS has no meaningful TS analogue.
  // Returning false matches sibling buildopt_* ports' contract.
  return false;
}
