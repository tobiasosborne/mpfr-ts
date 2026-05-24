/**
 * reference_ports/broken/mpfr_overflow.ts — deliberately-buggy mpfr_overflow.
 *
 * Used to mutation-prove the golden master per PIL.3.
 *
 * **Deliberately broken: inverts the IS_LIKE_RNDZ branch.** The C code
 * (L435-L444) chooses between mpfr_setmax (toward-zero) and SET_INF
 * (away-from-zero); this broken port swaps them — every away-from-zero
 * case gets the ±max-finite value and ternary=-1, and every toward-zero
 * case gets ±Inf and ternary=+1 (then sign-multiplied).
 *
 * This flips both `value.kind` and `ternary` on every case, dropping
 * composite to near zero.
 *
 * Why this bug shape:
 *   - A hurried agent reading the C body sees two branches and might
 *     pattern-match "the simpler branch first" by reading top-down,
 *     accidentally swapping which branch maps to which condition.
 *   - The C condition `MPFR_IS_LIKE_RNDZ(rnd, sign<0)` is unusual —
 *     a TS-naive reader might forget to invert the boolean when
 *     dispatching, producing exactly this swap.
 *   - Both branches end up writing to x with valid (kind, sign), so
 *     the schema-violation check passes — the bug is purely behavioural.
 *
 * NOT used in production.
 *
 * Ref: docs/PILOT_PLAN.md Step 8.
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/ops/overflow.ts — the correct version.
 */

import type { MPFR, Result, RoundingMode, Sign, Ternary } from '../../../src/core.ts';
import {
  MPFRError,
  negInf,
  posInf,
  PREC_MAX,
  PREC_MIN,
} from '../../../src/core.ts';
import { mpfr_setmax } from '../../../src/ops/setmax.ts';

const EMAX_DEFAULT: bigint = (1n << 30n) - 1n;

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
      `mpfr_overflow(broken): unknown rounding mode '${String(rnd)}'`,
    );
  }
  if (sign !== 1 && sign !== -1) {
    throw new MPFRError(
      'EPREC',
      `mpfr_overflow(broken): sign must be 1 or -1, got ${String(sign)}`,
    );
  }
}

export function mpfr_overflow(
  prec: bigint,
  rnd: RoundingMode,
  sign: Sign,
): Result {
  validateArgs(prec, rnd, sign);
  // BUG: invert the IS_LIKE_RNDZ predicate. Every branch now goes to
  // the wrong arm, producing wrong value AND wrong (sign-multiplied)
  // ternary on every case.
  const towardZero = !isLikeRNDZ(rnd, sign);
  let value: MPFR;
  let inex: Ternary;
  if (towardZero) {
    value = mpfr_setmax(prec, EMAX_DEFAULT, sign);
    inex = -1;
  } else {
    value = sign === 1 ? posInf(prec) : negInf(prec);
    inex = 1;
  }
  const ternary: Ternary = (sign === 1 ? inex : -inex) as Ternary;
  return { value, ternary };
}
