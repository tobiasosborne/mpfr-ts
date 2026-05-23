/**
 * Step-5 acceptance port (c): infinite loop.
 *
 * Synchronously loops forever on every call. Exercises the parent-side
 * `worker.terminate()` timeout path: the worker has no internal timeout
 * (CLAUDE.md Rule 4 + ../auto-port-eval/HANDOFF.md §2 — a
 * worker-internal AbortController cannot interrupt a synchronous
 * busy-loop because the event loop never yields). The runner races the
 * worker's reply against a class-tier ms budget and terminates the
 * worker on expiry.
 *
 * Expected grade on a 5–10-case golden: `n_infloop == n_cases`,
 * `wall_ms < 30000` (NOT >60000). Class is `arithmetic` → 200ms budget,
 * so total wall time for 5 cases is bounded by ~1s of timeout per case
 * plus worker respawn overhead, well under 30s.
 *
 * The `while (true) {}` body has type `never` from TS's control-flow
 * analysis, so the absence of a `return` is fine: the function's
 * declared return type `Result` is trivially satisfied by an
 * unreachable path. No `// @ts-expect-error` or `never` casts required.
 *
 * RED-phase scaffolding. Runner not yet implemented.
 */

import type { MPFR, Result } from '../../../../src/core.ts';

/**
 * Infinite synchronous loop. The `x` parameter is unused; we still
 * declare it to keep the function signature byte-identical to the
 * reference port so spec.json's positional-arg extraction sees the same
 * shape.
 *
 * `noUnusedParameters` is satisfied by the leading-underscore name.
 */
export function acceptanceFn(_x: MPFR): Result {
  // Synchronous busy-loop. Never yields to the event loop, so neither an
  // AbortController nor a worker-internal setTimeout can interrupt it —
  // only `worker.terminate()` from the parent process can. This is the
  // single largest failure mode the runner's worker-pool design exists
  // to address.
  while (true) {
    // Empty body; TS infers `never` for the loop and the rest of the
    // function is unreachable, so the missing `return { ... }` is fine
    // under noImplicitReturns.
  }
}
