/**
 * reference_ports/broken/mpfr_flags_set.ts -- deliberately-buggy.
 *
 * **BUG: returns AND instead of OR.** Treats set as intersection.
 * Most cases differ from expected.
 *
 * NOT used in production. Do NOT fix.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;

export function mpfr_flags_set(pre: bigint, mask: bigint): bigint {
  if (typeof pre !== 'bigint' || typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_flags_set: bad input`);
  }
  // BUG: should be OR; AND collapses to intersection.
  return (pre & mask) & MPFR_FLAGS_ALL;
}
