/**
 * reference_ports/correct/mpfr_init2.ts — re-export of the production port.
 *
 * The "correct" reference port for `mpfr_init2` IS the production
 * implementation under `src/ops/init2.ts`. This re-export exists so the
 * reference-port directory has symmetric `correct/<fn>.ts` and
 * `broken/<fn>.ts` paths per docs/PILOT_PLAN.md Step 7 — the harness
 * can run identical commands against both with only the directory
 * prefix changed.
 *
 * Do NOT duplicate the implementation here. Any divergence between
 * `correct/mpfr_init2.ts` and `src/ops/init2.ts` would be a
 * source-of-truth bug: the production op is the reference, full stop.
 * If the reference needs to change, change the production file.
 *
 * Ref: docs/PILOT_PLAN.md Step 7 — reference-port directory layout.
 * Ref: CLAUDE.md PIL.3 — mutation-prove the golden against this file.
 */

// Imports from core.ts are load-bearing for the schema gate:
// ast_check.ts requires every non-substrate port to name at least one
// symbol from `core.ts`, and a bare `export { ... } from "..."` re-export
// statement does not match the named-import regex it scans for. We
// import the locked-schema type via the SEPARATE `import type` form;
// the inline-`type` brace form would trip the redecl-pattern regex
// because that regex does not strip comments before scanning. The
// wrapper below is a transparent pass-through to the production
// implementation — do NOT add logic here; "correct === production"
// must hold exactly.
import type { MPFR } from '../../../src/core.ts';
import { mpfr_init2 as _impl } from '../../../src/ops/init2.ts';

export function mpfr_init2(prec: bigint): MPFR {
  return _impl(prec);
}



