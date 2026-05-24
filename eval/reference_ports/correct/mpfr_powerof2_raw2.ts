/**
 * reference_ports/correct/mpfr_powerof2_raw2.ts — re-export of the
 * production substrate port.
 *
 * Per the symmetric `correct/` <-> `broken/` layout, the "correct"
 * reference IS the production implementation — here that's the
 * substrate helper under `src/internal/mpfr/powerof2_raw2.ts`. This
 * wrapper exists so the harness can run identical commands against
 * `correct/<fn>.ts` and `broken/<fn>.ts` by changing only the directory
 * prefix.
 *
 * Do NOT duplicate the implementation here. Any divergence is a
 * source-of-truth bug: the substrate file IS the reference.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 * Ref: CLAUDE.md Law 3 — substrate is the faithful layer; this re-export
 *   crosses the substrate/grader boundary so the same harness invocation
 *   pattern works for substrate and surface ports alike.
 */

export { mpfr_powerof2_raw2 } from '../../../src/internal/mpfr/powerof2_raw2.ts';
