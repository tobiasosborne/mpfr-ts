/**
 * reference_ports/correct/mpfr_const_log2_internal.ts -- mutation-prove reference.
 *
 * Pragmatic reference: uses a precomputed 2048-bit log(2) constant
 * (computed by libmpfr at MPFR_RNDN, MSB-aligned to 2048 bits) and
 * performs correct rounding to the requested precision via the standard
 * rules for the 5 rounding modes.
 *
 * The PRODUCTION port should faithfully implement the binary-splitting
 * Ziv loop from mpfr/src/const_log2.c L107-L176. This reference exists
 * purely to calibrate the golden grader at composite=1.0 for prec <= 1024.
 *
 * Algorithm:
 *   1. Take the 2048-bit mantissa.
 *   2. Compute the round bit (bit at position 2048-prec-1) and the
 *      sticky bit (OR of bits below).
 *   3. Truncate to prec bits (shift right by 2048-prec).
 *   4. Apply rounding rule based on rnd, sign (+1 always for log(2)),
 *      round bit, sticky.
 *   5. Compute ternary: 0 if exact (round=sticky=0), +1 if rounded up,
 *      -1 if rounded down.
 *   6. Carry: if rounding overflows the prec window (mant becomes 2^prec
 *      exactly), shift right by 1 and bump exp.
 *
 * log(2) is in [0.5, 1.0), so unbiased exp = 0.
 *
 * Ref: mpfr/src/const_log2.c L107-L176 -- C reference (full algorithm).
 * Ref: CLAUDE.md 'Rounding modes are 5' -- handle all 5 modes correctly.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

/** log(2) MSB-aligned mantissa at 2048 bits of precision (RNDN). */
const LOG2_MANT_2048N: bigint = 22400441642467859577488739781209226459403626065886660720528601240145527492420725336291667021425889702299778972254785424771724351736870381509034525077594234322856729913233103191693932917751924172625994619620400660039442608528415575721354420972082577163443760602448842212312094033841292568983687855542170368247819631661533385799671122421173221385317065110553753226746392818491381768398893626906223910200995167915538026273569853823698607422800781837962733502154433664101279894325772997090164915246519231150831430415653842396495805010817059154674077638814850832366084056018734798876353421653877927763165084799176609677707n;
const LOG2_PREC_REF = 2048n;
const LOG2_EXP = 0n;  /* mpfr unbiased exponent for log(2) in [0.5, 1.0) */

const VALID_RNDS: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];

export function mpfr_const_log2_internal(prec: bigint, rnd: RoundingMode): Result {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_const_log2_internal: prec must be bigint`);
  }
  if (prec < 1n) {
    throw new MPFRError('EPREC', `mpfr_const_log2_internal: prec must be >= 1, got ${prec}`);
  }
  if (prec > LOG2_PREC_REF - 100n) {
    throw new MPFRError(
      'EPREC',
      `mpfr_const_log2_internal: prec > ${LOG2_PREC_REF - 100n} is out of scope for the reference`,
    );
  }
  if (!VALID_RNDS.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_const_log2_internal: unknown rnd ${String(rnd)}`);
  }

  // log(2) is positive; sign = +1 always.
  const sign = 1 as const;
  // Shift amount to extract top `prec` bits.
  const shift = LOG2_PREC_REF - prec;
  // The top `prec` bits.
  let truncMant = LOG2_MANT_2048N >> shift;
  // The round bit (bit just below truncation point) and sticky (OR of remaining bits).
  const roundBitPos = shift - 1n;
  const roundBit = (LOG2_MANT_2048N >> roundBitPos) & 1n;
  const stickyMask = (1n << roundBitPos) - 1n;
  const sticky = (LOG2_MANT_2048N & stickyMask) !== 0n;

  // Determine rounding direction.
  // For positive values:
  //   RNDN: round up iff (round=1 AND (sticky OR truncMant&1)) -- ties to even.
  //   RNDZ: never round up (truncate).
  //   RNDU: round up iff any bit was discarded (round | sticky).
  //   RNDD: never round up (truncate, since result > 0).
  //   RNDA: round up iff any bit was discarded (away from zero, positive).
  let roundUp = false;
  if (rnd === 'RNDN') {
    if (roundBit === 1n) {
      if (sticky) roundUp = true;
      else roundUp = (truncMant & 1n) === 1n;  // ties to even
    }
  } else if (rnd === 'RNDZ' || rnd === 'RNDD') {
    roundUp = false;
  } else if (rnd === 'RNDU' || rnd === 'RNDA') {
    roundUp = roundBit === 1n || sticky;
  }

  let exp = LOG2_EXP;
  if (roundUp) {
    truncMant += 1n;
    // Carry: if mantissa overflows the prec window.
    if (truncMant === 1n << prec) {
      truncMant = 1n << (prec - 1n);
      exp += 1n;
    }
  }

  // Ternary: sign of (rounded - exact). exact = log(2); rounded = output.
  // If we rounded up, output > exact (for positive). If we truncated and any
  // bits were nonzero, output < exact.
  let ternary: Ternary;
  const wasExact = roundBit === 0n && !sticky;
  if (wasExact) ternary = 0;
  else if (roundUp) ternary = 1;
  else ternary = -1;

  const value: MPFR = { kind: 'normal', sign, prec, exp, mant: truncMant };
  return { value, ternary };
}
