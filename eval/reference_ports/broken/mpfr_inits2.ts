/**
 * reference_ports/broken/mpfr_inits2.ts -- deliberately-buggy.
 *
 * **Collapses output to a constant 0n** regardless of inputs. Every
 * non-zero n fails strict equality; composite well below 0.30.
 */

import { MPFRError, PREC_MAX, PREC_MIN } from '../../../src/core.ts';

export function mpfr_inits2(prec: bigint, n: bigint): bigint {
  if (typeof prec !== 'bigint' || prec < PREC_MIN || prec > PREC_MAX) {
    throw new MPFRError('EPREC', `mpfr_inits2: bad prec`);
  }
  if (typeof n !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_inits2: bad n`);
  }
  return 0n;
}
