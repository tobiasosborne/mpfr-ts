/**
 * Step-5 acceptance port (e): NaN-equality.
 *
 * Behaviourally identical to ports/correct.ts. The NaN handling lives
 * entirely in the runner / value_codec layer: when the input is NaN
 * (kind === 'nan'), the codec folds it to the canonical NAN_VALUE on
 * both sides (decodeMpfr() in value_codec.ts) and compareMpfr() returns
 * a match on (NaN, NaN) without invoking field-by-field equality
 * (CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN").
 *
 * The golden for this scenario carries 5 NaN-input cases. Each
 * round-trips NaN through identity, and the runner must grade
 * composite >= 0.95. Anything less indicates the codec is incorrectly
 * applying structural equality to NaN values.
 *
 * RED-phase scaffolding. Runner not yet implemented.
 */

import type { MPFR, Result, Ternary } from '../../../../src/core.ts';

/**
 * Identity-on-input, same as correct.ts. Re-exported under a distinct
 * file path so the runner's `--port` flag selects this specific port
 * for the NaN scenario.
 */
export function acceptanceFn(x: MPFR): Result {
  const ternary: Ternary = 0;
  return { value: x, ternary };
}
