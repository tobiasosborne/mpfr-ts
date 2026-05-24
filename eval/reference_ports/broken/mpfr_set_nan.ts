/**
 * reference_ports/broken/mpfr_set_nan.ts — deliberately-buggy mpfr_set_nan.
 *
 * Used to mutation-prove the golden master per CLAUDE.md PIL.3. If
 * this port scores composite > 0.5 on the mpfr_set_nan golden, the
 * golden is too weak.
 *
 * **Deliberately broken: returns posZero(53n) instead of NAN_VALUE.**
 * This is a plausible agent mistake — confusing "the function that
 * starts a fresh MPFR" (mpfr_init2 / mpfr_set_zero) with
 * "the function that produces NaN". A port that returns +0 satisfies
 * `validate()` and looks reasonable on the wire, but fails every
 * compareMpfr() against the expected NaN sentinel because
 * actual.kind === 'zero' !== expected.kind === 'nan'.
 *
 * NOT used in production. NOT imported from src/. Do NOT fix.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/set_nan.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { posZero } from '../../../src/core.ts';

export function mpfr_set_nan(): MPFR {
  // BUG: should return NAN_VALUE. Returns +0 at prec 53 instead.
  return posZero(53n);
}
