/**
 * reference_ports/broken/mpfr_set_inf.ts — deliberately-buggy mpfr_set_inf.
 *
 * Used to mutation-prove the golden master per CLAUDE.md PIL.3.
 *
 * **Deliberately broken: flips the sign — sign=+1 produces -Inf,
 * sign=-1 produces +Inf.** Mirrors the "I got the sign convention
 * backwards" agent error. Symmetric to the broken mpfr_set_zero.
 * Every case fails on the `sign mismatch` branch of compareMpfr.
 *
 * NOT used in production. NOT imported from src/. Do NOT fix.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/set_inf.ts — the correct version.
 */

import type { MPFR, Sign } from '../../../src/core.ts';
import { negInf, posInf } from '../../../src/core.ts';

export function mpfr_set_inf(prec: bigint, sign: Sign): MPFR {
  // BUG: sign convention inverted — returns -Inf for sign=+1 and
  // +Inf for sign=-1.
  return sign === 1 ? negInf(prec) : posInf(prec);
}
