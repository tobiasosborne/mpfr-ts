/**
 * reference_ports/broken/mpfr_flags_restore.ts -- deliberately-buggy.
 *
 * **BUG: always returns 0n.** Strongest perturbation: collapse the
 * entire restore-logic decision tree to a constant. Strengthened from
 * earlier "ignore mask, return flags & ALL" variant (which scored 0.48
 * because for many cases flags happened to equal the expected restored
 * value).
 */

import { MPFRError } from '../../../src/core.ts';

export function mpfr_flags_restore(pre: bigint, flags: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint' || typeof flags !== 'bigint' || typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_restore: bad input`);
  }
  void pre; void flags; void mask;
  return 0n;
}
