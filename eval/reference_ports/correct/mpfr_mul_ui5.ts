/**
 * reference_ports/correct/mpfr_mul_ui5.ts -- mutation-prove reference.
 *
 * Compute y = x * v1 * v2 * v3 * v4 * v5 via the C MPFR_ACC_OR_MUL
 * accumulator-with-flush pattern (mpfr/src/gammaonethird.c L25-L62).
 *
 * Algorithm (literal mirror):
 *   acc = v1
 *   y = mpfr_set(x, prec, rnd)
 *   for v in [v2, v3, v4, v5]:
 *     if v <= ULONG_MAX / acc:
 *       acc *= v
 *     else:
 *       (y, _) = mpfr_mul_ui(y, acc, prec, rnd)  -- flush
 *       acc = v
 *   return mpfr_mul_ui(y, acc, prec, rnd)  -- final, returns ternary
 *
 * The ULONG_MAX threshold is preserved exactly so the flush pattern
 * matches C; using a different threshold would produce a different
 * rounding trail.
 *
 * Ref: mpfr/src/gammaonethird.c L49-L62 -- C mul_ui5 body.
 * Ref: mpfr/src/gammaonethird.c L25-L35 -- MPFR_ACC_OR_MUL macro.
 * Ref: src/ops/mul_ui.ts -- the underlying delegate.
 * Ref: src/ops/set.ts -- the initial copy.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from '../../../src/core.ts';
import { mpfr_mul_ui } from '../../../src/ops/mul_ui.ts';
import { mpfr_set } from '../../../src/ops/set.ts';

/** ULONG_MAX on LP64 (the platforms we care about): 2^64 - 1. */
const ULONG_MAX = (1n << 64n) - 1n;

function validateUlong(v: bigint, name: string): void {
  if (typeof v !== 'bigint') {
    throw new MPFRError('EDOMAIN', `${name} must be bigint, got ${typeof v}`);
  }
  if (v <= 0n) {
    // v_i = 0 is UB in the C source (the ACC_OR_MUL macro divides by acc
    // which becomes 0). Reject explicitly to keep the contract well-defined.
    throw new MPFRError('EDOMAIN', `${name} must be > 0 (C UB on 0), got ${v}`);
  }
  if (v > ULONG_MAX) {
    throw new MPFRError('EDOMAIN', `${name} must be <= ULONG_MAX (2^64-1), got ${v}`);
  }
}

function validateArgs(
  x: MPFR,
  v1: bigint, v2: bigint, v3: bigint, v4: bigint, v5: bigint,
  prec: bigint,
  rnd: RoundingMode,
): void {
  validateUlong(v1, 'v1');
  validateUlong(v2, 'v2');
  validateUlong(v3, 'v3');
  validateUlong(v4, 'v4');
  validateUlong(v5, 'v5');
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
    rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EPREC', 'x must be an MPFR value');
  }
}

export function mpfr_mul_ui5(
  x: MPFR,
  v1: bigint, v2: bigint, v3: bigint, v4: bigint, v5: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(x, v1, v2, v3, v4, v5, prec, rnd);

  // Initial: y = mpfr_set(x, prec, rnd). The intermediate ternary
  // is discarded -- the C macro chain only returns the final ternary.
  let y: MPFR = mpfr_set(x, prec, rnd).value;
  let acc = v1;

  // ACC_OR_MUL chain for v2..v5. Per validateUlong, all v_i > 0, so
  // acc never reaches 0 and the C UB-on-zero edge is unreachable.
  const stepAccOrMul = (v: bigint): void => {
    if (v <= ULONG_MAX / acc) {
      acc *= v;
    } else {
      y = mpfr_mul_ui(y, acc, prec, rnd).value;
      acc = v;
    }
  };
  stepAccOrMul(v2);
  stepAccOrMul(v3);
  stepAccOrMul(v4);
  stepAccOrMul(v5);

  // Final mpfr_mul_ui returns the ternary we propagate.
  return mpfr_mul_ui(y, acc, prec, rnd);
}
