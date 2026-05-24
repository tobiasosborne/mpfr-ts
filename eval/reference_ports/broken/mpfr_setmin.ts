/**
 * reference_ports/broken/mpfr_setmin.ts — deliberately-buggy mpfr_setmin.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_setmin golden, the golden is too weak.
 *
 * **Deliberately broken: produces the maximum mantissa instead of the
 * minimum.** The mirror of the broken setmax — same agent error in the
 * opposite direction. Correct min mantissa is `2^(prec - 1)` (MSB only);
 * the broken port returns `2^prec - 1` (all bits set, the setmax value).
 *
 * Both values pass `validate()` — they sit at opposite ends of the
 * MSB-aligned range `[2^(prec-1), 2^prec)`. Only direct comparison
 * against the libmpfr-emitted golden mantissa catches the swap.
 *
 * Why this bug shape (same rationale as broken setmax, inverted):
 *   - setmax.c and setmin.c live next to each other and look similar.
 *   - The mantissa formulas differ in one character in TS.
 *   - An agent porting both functions in a batch could swap them.
 *
 * Edge case: at PREC_MIN (prec = 1), the all-ones mantissa `2^1 - 1 = 1`
 * equals the MSB-only mantissa `2^0 = 1`, so the broken port produces
 * the right answer at prec=1. This is fine for mutation-prove purposes;
 * the golden has plenty of prec >= 2 cases.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.55 under mutation.
 * Ref: src/ops/setmin.ts — the correct version.
 */

import type { MPFR, Sign } from '../../../src/core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../../../src/core.ts';

function validateArgs(prec: bigint, exp: bigint, sign: Sign): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (typeof exp !== 'bigint') {
    throw new MPFRError('EPREC', `exp must be bigint, got ${typeof exp}`);
  }
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_setmin(broken): sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

export function mpfr_setmin(
  prec: bigint,
  exp: bigint,
  sign: Sign = 1,
): MPFR {
  validateArgs(prec, exp, sign);
  // BUG: should be `1n << (prec - 1n)` (the MSB-only mantissa).
  // Returns the all-ones mantissa instead (a setmax shape).
  const mant = (1n << prec) - 1n;
  return { kind: 'normal', sign, prec, exp, mant };
}
