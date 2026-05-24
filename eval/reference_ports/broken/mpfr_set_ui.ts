/**
 * reference_ports/broken/mpfr_set_ui.ts — deliberately-buggy mpfr_set_ui.
 *
 * **Deliberately broken: returns posZero(prec) for every input.** The
 * port silently collapses every unsigned integer to zero. Bug shape:
 * a stub that "compiles" but does no real work.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: src/ops/set_ui.ts — the correct version.
 */

import type { Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
  posZero,
} from '../../../src/core.ts';

const ULONG_MAX_VAL: bigint = (1n << 64n) - 1n;

function validateArgs(n: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof n !== 'bigint') {
    throw new MPFRError('EPREC', `n must be bigint, got ${typeof n}`);
  }
  if (n < 0n || n > ULONG_MAX_VAL) {
    throw new MPFRError(
      'EPREC',
      `n out of uint64 range [0, ${ULONG_MAX_VAL}], got ${n}`,
    );
  }
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (
    rnd !== 'RNDN' &&
    rnd !== 'RNDZ' &&
    rnd !== 'RNDU' &&
    rnd !== 'RNDD' &&
    rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

export function mpfr_set_ui(
  n: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(n, prec, rnd);
  // BUG: always return posZero(prec), regardless of n's value.
  void n;
  return { value: posZero(prec), ternary: 0 };
}
