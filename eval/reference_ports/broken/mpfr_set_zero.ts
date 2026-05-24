/**
 * reference_ports/broken/mpfr_set_zero.ts — deliberately-buggy mpfr_set_zero.
 *
 * Used to mutation-prove the golden master per CLAUDE.md PIL.3.
 *
 * **Deliberately broken: flips the sign — sign=+1 produces -0, sign=-1
 * produces +0.** Mirrors the "I got the sign convention backwards"
 * agent error (the C side has `if (sign < 0) MPFR_SET_NEG(x)` and a
 * naive port might invert that condition). Every case fails on the
 * `sign mismatch` branch of compareMpfr — the bug is complete, so the
 * composite drops well below 0.5.
 *
 * NOT used in production. NOT imported from src/. Do NOT fix.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: CLAUDE.md "Hallucination-risk callouts: Signed zero is real".
 * Ref: src/ops/set_zero.ts — the correct version.
 */

import type { MPFR, Sign } from '../../../src/core.ts';
import { negZero, posZero } from '../../../src/core.ts';

export function mpfr_set_zero(prec: bigint, sign: Sign): MPFR {
  // BUG: sign convention inverted — returns -0 for sign=+1 and +0
  // for sign=-1. A port that does this misses the entire point of
  // signed zero being observable in MPFR.
  return sign === 1 ? negZero(prec) : posZero(prec);
}
