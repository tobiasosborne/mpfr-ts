/**
 * reference_ports/correct/mpfr_compound_near_one.ts -- mutation-prove reference.
 *
 * Algorithm (mpfr/src/compound.c L31-L54):
 *   y = 1 (exact at given prec)
 *   if rnd == RNDN
 *      OR (s > 0 AND rnd in {RNDZ, RNDD})
 *      OR (s < 0 AND rnd in {RNDA, RNDU}):
 *     return -s
 *   else if s > 0:  // must be RNDA or RNDU
 *     y = nextabove(y); return +1
 *   else:           // s < 0, must be RNDZ or RNDD
 *     y = nextbelow(y); return -1
 *
 * Delegates to mpfr_nextabove / mpfr_nextbelow (already shipped).
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';
import { mpfr_nextabove } from '../../../src/ops/nextabove.ts';
import { mpfr_nextbelow } from '../../../src/ops/nextbelow.ts';

const VALID_RNDS: readonly RoundingMode[] = ['RNDN', 'RNDZ', 'RNDU', 'RNDD', 'RNDA'];

// Build the MPFR value "1" at the requested prec: kind=normal, sign=+1,
// mant = 2^(prec-1) (MSB only), exp = 1 (value = 1 * 2^(1 - prec) * 2^(prec-1) = 1).
function buildOne(prec: bigint): MPFR {
  return {
    kind: 'normal',
    sign: 1,
    prec,
    exp: 1n,
    mant: 1n << (prec - 1n),
  };
}

export function mpfr_compound_near_one(prec: bigint, s: number, rnd: RoundingMode): Result {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `mpfr_compound_near_one: prec must be bigint`);
  }
  if (prec < 1n) {
    throw new MPFRError('EPREC', `mpfr_compound_near_one: prec must be >= 1, got ${prec}`);
  }
  if (s !== +1 && s !== -1) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_compound_near_one: s must be +1 or -1, got ${s}`,
    );
  }
  if (!VALID_RNDS.includes(rnd)) {
    throw new MPFRError('EROUND', `mpfr_compound_near_one: unknown rnd ${String(rnd)}`);
  }

  let y = buildOne(prec);

  // Round-to-1 branch.
  if (
    rnd === 'RNDN' ||
    (s > 0 && (rnd === 'RNDZ' || rnd === 'RNDD')) ||
    (s < 0 && (rnd === 'RNDA' || rnd === 'RNDU'))
  ) {
    return { value: y, ternary: (-s) as Ternary };
  }
  // Step away.
  if (s > 0) {
    // Must be RNDA or RNDU
    y = mpfr_nextabove(y);
    return { value: y, ternary: 1 };
  }
  // s < 0, must be RNDZ or RNDD
  y = mpfr_nextbelow(y);
  return { value: y, ternary: -1 };
}
