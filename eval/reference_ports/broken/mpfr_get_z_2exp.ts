/**
 * reference_ports/broken/mpfr_get_z_2exp.ts -- deliberately-buggy port.
 *
 * **Deliberately broken: always returns {z: 0n, exp: 0n}** -- the
 * simplest stub mistake. Only the +/-0 cases happen to pass; every
 * nonzero input fails the field comparison.
 *
 * Per worklog 018 lesson, "collapse to constant" beats narrow
 * perturbation because narrow perturbations leave too many cases
 * passing.
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: composite <= 0.5.
 *
 * Ref: eval/reference_ports/correct/mpfr_get_z_2exp.ts -- the correct
 *   version.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError, validate } from '../../../src/core.ts';

export function mpfr_get_z_2exp(x: MPFR): { z: bigint; exp: bigint } {
  validate(x);
  // Preserve the singular handling (NaN/Inf throw, +/-0 returns
  // {0n, 0n}) so the bug is narrow to the value path; the broken port
  // still has the correct DIVERGENCE shape against the C reference.
  if (x.kind === 'nan') {
    throw new MPFRError('EPREC', 'mpfr_get_z_2exp: NaN');
  }
  if (x.kind === 'inf') {
    throw new MPFRError('EPREC', 'mpfr_get_z_2exp: Inf');
  }
  // BUG: always returns {0n, 0n}, ignoring x.
  return { z: 0n, exp: 0n };
}
