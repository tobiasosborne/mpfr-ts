/**
 * reference_ports/broken/mpfr_erangeflag_p.ts -- deliberately-buggy.
 *
 * **Multi-bug perturbation (per worklog 006 #6).** Polarity flip plus
 * low-bit force-true to drive agreement well below 0.30. Targets the
 * fact that half of cases would otherwise pass under a naive flip
 * (correct returns false on most cases since ERANGE is not the
 * dominant bit).
 */

import { MPFRError } from '../../../src/core.ts';

const ERANGE_BIT = 16n;

export function mpfr_erangeflag_p(mask: bigint): boolean {
  if (typeof mask !== 'bigint') {
    throw new MPFRError('EDOMAIN', `mpfr_erangeflag_p: bad input`);
  }
  // BUG 2: low-bit masks force-true to anti-correlate with correct answer.
  if (mask < 16n) return true;
  // BUG 1: polarity flip (XOR-obfuscated).
  return ((mask ^ 16n) & ERANGE_BIT) !== 0n;
}
