/**
 * reference_ports/broken/mpfr_const_log2_internal.ts -- deliberately-buggy.
 *
 * **BUG: returns log(2) at the wrong exponent (exp = 1 instead of 0).**
 * The value claims to represent log(2) but at twice the correct magnitude.
 * Every case mismatches.
 */

import type { MPFR, Result, RoundingMode } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

const LOG2_MANT_2048N: bigint = 22400441642467859577488739781209226459403626065886660720528601240145527492420725336291667021425889702299778972254785424771724351736870381509034525077594234322856729913233103191693932917751924172625994619620400660039442608528415575721354420972082577163443760602448842212312094033841292568983687855542170368247819631661533385799671122421173221385317065110553753226746392818491381768398893626906223910200995167915538026273569853823698607422800781837962733502154433664101279894325772997090164915246519231150831430415653842396495805010817059154674077638814850832366084056018734798876353421653877927763165084799176609677707n;
const LOG2_PREC_REF = 2048n;
const VALID_RNDS: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];

export function mpfr_const_log2_internal(prec: bigint, rnd: RoundingMode): Result {
  if (typeof prec !== 'bigint' || prec < 1n) {
    throw new MPFRError('EPREC', 'bad prec');
  }
  if (prec > LOG2_PREC_REF - 100n) {
    throw new MPFRError('EPREC', 'prec out of scope');
  }
  if (!VALID_RNDS.includes(rnd)) {
    throw new MPFRError('EROUND', 'bad rnd');
  }
  const shift = LOG2_PREC_REF - prec;
  const truncMant = LOG2_MANT_2048N >> shift;
  return {
    value: {
      kind: 'normal',
      sign: 1,
      prec,
      exp: 1n,  // BUG: should be 0n -- log(2) is in [0.5, 1.0).
      mant: truncMant,
    },
    ternary: 0,  // BUG: ternary not properly computed either.
  };
}
