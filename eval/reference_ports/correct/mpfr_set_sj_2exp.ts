/**
 * reference_ports/correct/mpfr_set_sj_2exp.ts -- mutation-prove reference.
 *
 * Sets the result to j * 2^e, correctly rounded. j is a signed int64
 * (intmax_t analog) constrained to [-(2^63), 2^63 - 1].
 *
 * Algorithm (mpfr/src/set_sj.c L34-L45):
 *   if (j >= 0) return mpfr_set_uj_2exp(rop, j, e, rnd);
 *   else {
 *     inex = mpfr_set_uj_2exp(rop, -(uintmax_t)j, e, INVERT_RND(rnd));
 *     CHANGE_SIGN(rop);
 *     return -inex;
 *   }
 *
 * The TS port mirrors this. For j = INT64_MIN, JS bigint negation
 * `-(-9223372036854775808n)` yields `+9223372036854775808n` directly
 * (bigint has no overflow), matching the C `-(uintmax_t)j` semantics.
 *
 * Ref: mpfr/src/set_sj.c L27-L45 -- C reference.
 * Ref: mpfr/src/mpfr-impl.h L1249-L1250 -- MPFR_INVERT_RND.
 * Ref: src/ops/set_uj_2exp.ts -- the underlying delegate.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import {
  MPFRError,
  PREC_MAX,
  PREC_MIN,
} from '../../../src/core.ts';
import { mpfr_set_uj_2exp } from '../../../src/ops/set_uj_2exp.ts';

const INT64_MIN = -(1n << 63n);
const INT64_MAX = (1n << 63n) - 1n;

function invertRnd(rnd: RoundingMode): RoundingMode {
  if (rnd === 'RNDU') return 'RNDD';
  if (rnd === 'RNDD') return 'RNDU';
  return rnd;
}

function validateArgs(j: bigint, e: bigint, prec: bigint, rnd: RoundingMode): void {
  if (typeof j !== 'bigint') {
    throw new MPFRError('EPREC', `j must be bigint, got ${typeof j}`);
  }
  if (j < INT64_MIN) {
    throw new MPFRError('EDOMAIN', `j must be >= -(2^63) (intmax_t domain), got ${j}`);
  }
  if (j > INT64_MAX) {
    throw new MPFRError('EDOMAIN', `j must be <= 2^63 - 1 (intmax_t domain), got ${j}`);
  }
  if (typeof e !== 'bigint') {
    throw new MPFRError('EPREC', `e must be bigint, got ${typeof e}`);
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
    rnd !== 'RNDN' && rnd !== 'RNDZ' && rnd !== 'RNDU' && rnd !== 'RNDD' && rnd !== 'RNDA'
  ) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

export function mpfr_set_sj_2exp(
  j: bigint,
  e: bigint,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(j, e, prec, rnd);

  if (j >= 0n) {
    return mpfr_set_uj_2exp(j, e, prec, rnd);
  }

  // j < 0n: negate, invert rounding, flip sign of result, negate ternary.
  // Bigint `-j` is lossless even for INT64_MIN: -(-2^63n) === 2^63n which
  // is in [0n, UINT64_MAX] (UINT64_MAX = 2^64 - 1).
  const inner = mpfr_set_uj_2exp(-j, e, prec, invertRnd(rnd));

  // Flip sign on the result. inner.value is either +0 (j=0 case impossible
  // here since j < 0n), or +normal, or +Inf -- never NaN since the input
  // bigint is finite.
  let flipped: MPFR;
  if (inner.value.kind === 'zero') {
    // Should not be reachable when j < 0n -- defensive fallthrough.
    flipped = { ...inner.value, sign: -1 };
  } else if (inner.value.kind === 'nan') {
    flipped = inner.value;
  } else {
    flipped = { ...inner.value, sign: -1 };
  }

  const ternary = -inner.ternary as -1 | 0 | 1;
  return { value: flipped, ternary };
}
