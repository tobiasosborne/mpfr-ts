/**
 * ops/abort_prec_max.ts — pure-TS port of MPFR's `mpfr_abort_prec_max`.
 *
 * Status: **stub**. The function is deferred from grading because the
 * current harness cannot encode "expected throw" cases — every thrown
 * exception is classified as `n_throw`, which is the failure path. The
 * port is correct (it raises `MPFRError('EPREC', ...)`), but the
 * grader's wire codec has no way to compare the expected `MPFRError`
 * against the thrown value.
 *
 * Extending the codec to support a `expected_throw` wire tag is a
 * harness-level change tracked as a bd issue; once landed, this file
 * stays unchanged (the port is already correct) and a regular spec.json
 * + golden_driver.c gets added under `eval/functions/mpfr_abort_prec_max/`.
 *
 * C signature
 * -----------
 *
 *   MPFR_COLD_FUNCTION_ATTR MPFR_NORETURN void
 *   mpfr_abort_prec_max (void);
 *
 *   - Writes "MPFR: Maximal precision overflow\n" to stderr.
 *   - Calls `abort()` — process termination via SIGABRT.
 *
 *   Ref: mpfr/src/abort_prec_max.c L24–L29.
 *
 * TS signature
 * ------------
 *
 *   mpfr_abort_prec_max(): never;
 *
 *   - Throws `MPFRError('EPREC', "Maximal precision overflow")`. We
 *     translate `abort()` to a thrown exception because a port that
 *     crashes the process would corrupt the harness worker pool and
 *     leak through to the orchestrator as an unrecoverable failure.
 *     `MPFRError` with code `EPREC` (the bad-precision class) is the
 *     closest semantic fit: this function exists exactly to signal
 *     that a precision computation has overflowed the representable
 *     range.
 *
 * Why throw rather than `process.exit` or similar
 * -----------------------------------------------
 *
 * The library is pure ESM with no `node:*` or `Bun.*` imports (Rule 12);
 * `process.exit` is unavailable from `src/`. Even if it were available,
 * exiting the process is the wrong semantic in TS — a user who imports
 * this library and somehow reaches `mpfr_abort_prec_max` deserves a
 * recoverable exception, not a forced shutdown. `throw` is the
 * idiomatic translation of C's `abort()` for any code crossing a
 * runtime boundary.
 *
 * Why a separate `MPFRError` code is not added
 * --------------------------------------------
 *
 * The `MPFRErrorCode` enum (src/core.ts L187) is closed: `EPREC`,
 * `EROUND`, `EDOMAIN`. Adding a new variant requires an ADR
 * (CLAUDE.md Rule 14) and changes every consumer's pattern-match. The
 * existing `EPREC` (precision-domain failure) is a precise fit; the
 * message disambiguates the specific cause.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/abort_prec_max.c — the C reference.
 *   - src/core.ts L197–L209 — `MPFRError` class.
 *   - CLAUDE.md Law 4 — library coherence (no new error codes).
 *   - CLAUDE.md Rule 12 — no `node:*` / `Bun.*` imports in src/.
 */

import { MPFRError } from '../core.ts';

/**
 * Signal a precision-domain overflow. Throws unconditionally; never
 * returns (the return type `never` makes call sites narrow correctly
 * after the call).
 *
 * @mpfrName mpfr_abort_prec_max
 *
 * @throws {MPFRError} always — with `code === 'EPREC'` and the message
 *                    "Maximal precision overflow".
 */
export function mpfr_abort_prec_max(): never {
  throw new MPFRError('EPREC', 'Maximal precision overflow');
}
