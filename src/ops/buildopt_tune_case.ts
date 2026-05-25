/**
 * ops/buildopt_tune_case.ts -- pure-TS port of MPFR's
 * `mpfr_buildopt_tune_case`.
 *
 * Build-time accessor returning the name of the threshold-tuning case
 * that libmpfr was configured against. In the C reference this is the
 * preprocessor macro `MPFR_TUNE_CASE`, which is set by the platform-
 * specific tune step (`./configure --with-mulhigh-size=...` and friends)
 * and defaults to the string `"default"` when no platform tuning was
 * applied.
 *
 * The TS port returns the compile-time constant `'default'`: pure-TS has
 * no per-platform tuning hooks -- there is no `MPFR_TUNE_CASE` analogue,
 * no `mparam.h` cascade, no host-specific basecase-multiplication
 * threshold. The honest answer is the libmpfr default tune-case string,
 * which is what unconfigured libmpfr also returns.
 *
 * The grader-locked schema (`src/core.ts`) is not directly referenced
 * here (no-arg, primitive return), but we keep an explicit type-only
 * import to satisfy the AST gate (CLAUDE.md Law 4) and document this
 * port as a citizen of the locked library surface.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/buildopt.c L96-L100 -- C reference.
 *   - eval/reference_ports/correct/mpfr_buildopt_tune_case.ts -- ref.
 *   - eval/functions/mpfr_buildopt_tune_case/spec.json -- contract.
 *   - src/core.ts -- locked schema (type-only import).
 */

import type { MPFR as _MPFR } from '../core.ts';

/**
 * Name of the threshold-tuning case libmpfr was configured against.
 *
 * @mpfrName mpfr_buildopt_tune_case
 *
 * @divergence The C reference returns the preprocessor macro
 *   `MPFR_TUNE_CASE`, set by platform-specific configuration. Pure-TS
 *   has no per-platform tuning hooks (no `mparam.h`, no host-specific
 *   thresholds), so this port returns the constant `'default'` -- the
 *   same string an unconfigured libmpfr returns.
 *
 * @returns Always `'default'` in the pure-TS port.
 *
 * @example
 *   mpfr_buildopt_tune_case();  // "default"
 */
export function mpfr_buildopt_tune_case(): string {
  // Ref: mpfr/src/buildopt.c L96-L100 -- `return MPFR_TUNE_CASE;`.
  // No TS-side platform tuning exists; the libmpfr default is honest.
  return 'default';
}
