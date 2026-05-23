/**
 * Step-5 acceptance port (d): schema violator.
 *
 * This port intentionally violates Law 4 in two ways simultaneously:
 *
 *   1. It does NOT import any symbol from src/core.ts.
 *   2. It REDECLARES `MPFR` (as a local interface) and `Result` (as a
 *      local type alias).
 *
 * ast_check.ts is expected to reject this pre-flight (composite=0,
 * n_cases=0, schema_errors length > 0). The runner must NOT spawn a
 * worker for this port — the gate runs before any case dispatch.
 *
 * The exported function is still callable in TS terms (so the file
 * type-checks) but never executed by the runner.
 *
 * RED-phase scaffolding. Runner not yet implemented.
 *
 * Ref: eval/harness/ast_check.ts — REDECL_PATTERNS and requireCoreImport
 *   are the two gates this file fails.
 */

/**
 * Local redeclaration of {@link MPFR}. This is exactly the redeclaration
 * ast_check's REDECL_PATTERNS array forbids: `interface MPFR` matches
 * the `MPFR` pattern at a word boundary.
 */
interface MPFR {
  readonly kind: 'normal' | 'zero' | 'inf' | 'nan';
  readonly sign: 1 | -1;
  readonly prec: bigint;
  readonly exp: bigint;
  readonly mant: bigint;
}

/**
 * Local redeclaration of {@link Result}. Same violation pattern. Note
 * the property is `readonly` to mirror the locked schema; we want this
 * file to type-check cleanly so the violation is observable only at
 * the ast_check layer, not at `tsc --noEmit`.
 */
type Result = {
  readonly value: MPFR;
  readonly ternary: -1 | 0 | 1;
};

export function acceptanceFn(x: MPFR): Result {
  return { value: x, ternary: 0 };
}
