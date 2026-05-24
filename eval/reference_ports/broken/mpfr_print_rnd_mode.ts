/**
 * reference_ports/broken/mpfr_print_rnd_mode.ts — deliberately-buggy
 * mpfr_print_rnd_mode.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_print_rnd_mode golden, the golden is too weak.
 *
 * **Deliberately broken: rotate-shift the rounding-mode names by one
 * position.** Map: RNDN→RNDZ, RNDZ→RNDU, RNDU→RNDD, RNDD→RNDA,
 * RNDA→RNDN. Plausible agent error: the C-side switch lists modes
 * in the order D, U, N, Z, A — a hurried agent transcribing the table
 * could slide-shift the entries by one, producing exactly this rotation.
 *
 * Every case fails (no fixed points in the rotation), so composite
 * drops to ~0.0.
 *
 * Why this bug shape:
 *   - RNDZ and RNDA are easily confused in prose ("toward zero" vs
 *     "away from zero") — the polarity is opposite, easy to flip.
 *   - The C source orders modes D, U, N, Z, A in the switch — a
 *     hurried agent might transcribe the source order, then misread
 *     "Z" as the 4th mode (which it is in the C order) but emit the
 *     5th (A) tag, or vice versa.
 *   - Both modes return the same shape — just a string differing in
 *     one letter — so the typo wouldn't be caught by a hasty
 *     spot-check of one or two test cases.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/print_rnd_mode.ts — the correct version.
 */

import type { RoundingMode } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const VALID_MODES: readonly RoundingMode[] = Object.freeze([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
] as const);

export function mpfr_print_rnd_mode(rnd: RoundingMode): { readonly name: string } {
  if (!VALID_MODES.includes(rnd)) {
    throw new MPFRError(
      'EROUND',
      `mpfr_print_rnd_mode(broken): unknown rounding mode '${String(rnd)}'`,
    );
  }
  // BUG: rotate-shift the rounding-mode names.
  const ROT: Record<RoundingMode, string> = {
    RNDN: 'RNDZ',
    RNDZ: 'RNDU',
    RNDU: 'RNDD',
    RNDD: 'RNDA',
    RNDA: 'RNDN',
  };
  const label = ROT[rnd];
  return { name: `MPFR_${label}` };
}
