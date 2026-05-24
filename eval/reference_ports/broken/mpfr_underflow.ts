/**
 * reference_ports/broken/mpfr_underflow.ts — deliberately-buggy mpfr_underflow.
 *
 * Used to mutation-prove the golden master per PIL.3.
 *
 * **Deliberately broken: inverts the IS_LIKE_RNDZ branch.** Mirror image
 * of the broken mpfr_overflow bug: swaps the ±0 and ±min-finite branches,
 * producing the wrong value and ternary on every case. Composite drops
 * to near zero.
 *
 * Why this bug shape:
 *   - Same as overflow's broken port: easy to invert the branch
 *     dispatch when reading the C source top-down.
 *   - Symmetric to the overflow case, so the same mutation has a
 *     consistent failure-mode profile.
 *
 * NOT used in production.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/underflow.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import {
  MPFRError,
  negZero,
  posZero,
  PREC_MAX,
  PREC_MIN,
} from '../../../src/core.ts';
import { mpfr_setmin } from '../../../src/ops/setmin.ts';

const EMIN_DEFAULT: bigint = -((1n << 30n) - 1n);

const VALID_RND: readonly RoundingMode[] = Object.freeze([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
] as const);

function isLikeRNDZ(rnd: RoundingMode, sign: Sign): boolean {
  if (rnd === 'RNDZ') return true;
  if (rnd === 'RNDU' && sign === -1) return true;
  if (rnd === 'RNDD' && sign === 1) return true;
  return false;
}

function validateArgs(prec: bigint, rnd: RoundingMode, sign: Sign): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (!VALID_RND.includes(rnd)) {
    throw new MPFRError(
      'EROUND',
      `mpfr_underflow(broken): unknown rounding mode '${String(rnd)}'`,
    );
  }
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_underflow(broken): sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

export function mpfr_underflow(
  prec: bigint,
  rnd: RoundingMode,
  sign: Sign,
): Result {
  validateArgs(prec, rnd, sign);
  // BUG: invert the IS_LIKE_RNDZ predicate. Every branch now goes the
  // wrong way: toward-zero modes produce ±min-finite instead of ±0,
  // away-from-zero modes produce ±0 instead of ±min.
  const towardZero = !isLikeRNDZ(rnd, sign);
  let value: MPFR;
  let inex: Ternary;
  if (towardZero) {
    value = sign === 1 ? posZero(prec) : negZero(prec);
    inex = -1;
  } else {
    value = mpfr_setmin(prec, EMIN_DEFAULT, sign);
    inex = 1;
  }
  const ternary: Ternary = (sign === 1 ? inex : -inex) as Ternary;
  return { value, ternary };
}
