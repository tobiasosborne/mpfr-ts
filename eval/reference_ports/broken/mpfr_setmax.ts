/**
 * reference_ports/broken/mpfr_setmax.ts — deliberately-buggy mpfr_setmax.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_setmax golden, the golden is too weak.
 *
 * **Deliberately broken: produces the minimum mantissa instead of the
 * maximum.** A plausible agent error: confused `mpfr_setmax` with its
 * sibling `mpfr_setmin`. The correct max mantissa is `(2^prec - 1)`
 * (all `prec` bits set); the broken port returns `2^(prec - 1)` (only
 * the MSB set, the value MPFR's `setmin` produces).
 *
 * Both values pass `validate()` — they sit at opposite ends of the
 * MSB-aligned range `[2^(prec-1), 2^prec)`. So this bug is not caught
 * by structural validation; only by direct comparison against the
 * libmpfr-emitted golden mantissa.
 *
 * Why this bug shape:
 *   - The two functions live in adjacent files (setmax.c, setmin.c).
 *   - Their C implementations are visually nearly identical (one fills
 *     with MPFR_LIMB_MAX, the other with MPFR_LIMB_HIGHBIT at xn-1).
 *   - In the TS port the mantissa is a single bigint expression — one
 *     character (`-` vs `<<`) separates the two correct answers.
 *   - It's the kind of swap a hurried agent might make especially when
 *     porting both functions in one pass (which the orchestrator may
 *     well do).
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.55 under mutation.
 * Ref: src/ops/setmax.ts — the correct version.
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
      `mpfr_setmax(broken): sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

export function mpfr_setmax(
  prec: bigint,
  exp: bigint,
  sign: Sign = 1,
): MPFR {
  validateArgs(prec, exp, sign);
  // BUG: should be `(1n << prec) - 1n` (the all-ones mantissa).
  // Returns the smallest valid mantissa instead (a setmin shape).
  const mant = 1n << (prec - 1n);
  return { kind: 'normal', sign, prec, exp, mant };
}
