/**
 * misc/mpfr_mul_ui5.ts -- pure-TS port of MPFR's static `mpfr_mul_ui5`.
 *
 * Computes y = x * v1 * v2 * v3 * v4 * v5 using the MPFR_ACC_OR_MUL
 * accumulator-with-flush pattern. The rounding sequence matters: every
 * call to mpfr_mul_ui rounds at prec bits per the requested mode, so
 * the answer is NOT the same as a single multiplication by the product
 * (which would be exact if the product fits ULONG_MAX, but rounded
 * differently if it doesn't, and even if exact the intermediate
 * accumulator pattern produces a different ternary trail).
 *
 * C signature (static, inside mpfr/src/gammaonethird.c):
 *
 *   static void mpfr_mul_ui5 (mpfr_ptr y, mpfr_srcptr x,
 *     unsigned long int v1, unsigned long int v2, unsigned long int v3,
 *     unsigned long int v4, unsigned long int v5, mpfr_rnd_t mode);
 *
 * TS signature (this port):
 *
 *   mpfr_mul_ui5(x, v1, v2, v3, v4, v5, prec, rnd) -> Result
 *
 * Algorithm (literal mirror of the C body):
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
 * Ref: mpfr/src/gammaonethird.c L49-L62 -- C mul_ui5 body.
 * Ref: mpfr/src/gammaonethird.c L25-L35 -- MPFR_ACC_OR_MUL macro.
 * Ref: src/ops/mul_ui.ts -- the underlying delegate.
 * Ref: src/ops/set.ts -- the initial mpfr_set(y, x, mode).
 * Ref: src/core.ts -- locked schema.
 * Ref: docs/adr/0002-approximation-helper-grading.md -- golden-driver-substitute
 *   pattern for static helpers.
 * Ref: memory/decision_delegation_pattern.md -- standalone-wire-form-with-delegate.
 */

import type { MPFR, Result, RoundingMode } from "../core.ts";
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from "../core.ts";
import { mpfr_mul_ui } from "./mul_ui.ts";
import { mpfr_set } from "./set.ts";

/** ULONG_MAX on LP64: 2^64 - 1. */
const ULONG_MAX = (1n << 64n) - 1n;

function validateUlong(v: bigint, name: string): void {
  if (typeof v !== 'bigint') {
    throw new MPFRError('EDOMAIN', `${name} must be bigint, got ${typeof v}`);
  }
  if (v <= 0n) {
    // v_i = 0 is UB in the C source (ACC_OR_MUL divides by acc which
    // becomes 0). Reject explicitly to keep the contract well-defined.
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
  for (const v of [v2, v3, v4, v5]) {
    if (v <= ULONG_MAX / acc) {
      acc *= v;
    } else {
      y = mpfr_mul_ui(y, acc, prec, rnd).value;
      acc = v;
    }
  }

  // Final mpfr_mul_ui returns the ternary we propagate.
  return mpfr_mul_ui(y, acc, prec, rnd);
}
