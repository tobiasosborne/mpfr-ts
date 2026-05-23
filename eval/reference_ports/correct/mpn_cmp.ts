/**
 * reference_ports/correct/mpn_cmp.ts — re-export of the production port.
 *
 * The "correct" reference port for `mpn_cmp` IS the production substrate
 * implementation under `src/internal/mpn/cmp.ts` — that file is what
 * `mpfr_*` ops will import when substrate work scales out, and it is by
 * definition the canonical correct behaviour for Pilot mutation-proof
 * checks. This file exists solely so the reference-port directory has
 * symmetric `correct/<fn>.ts` and `broken/<fn>.ts` paths per
 * docs/PILOT_PLAN.md Step 7 — the harness can run identical commands
 * against both with only the directory prefix changed.
 *
 * Do NOT duplicate the implementation here. Any divergence between
 * `correct/mpn_cmp.ts` and `src/internal/mpn/cmp.ts` would be a
 * source-of-truth bug: the production substrate is the reference, full
 * stop. If the reference needs to change, change the substrate.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md Law 3 — substrate is the faithful layer.
 */

export { mpn_cmp } from '../../../src/internal/mpn/cmp.ts';
