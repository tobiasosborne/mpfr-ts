/**
 * reference_ports/broken/mpfr_fpif_store_exponent.ts -- deliberately-buggy.
 *
 * **BUG: collapses the entire encoding to a constant
 * { bytes: 0n, byte_length: 1 } regardless of input.** Per HANDOFF
 * gotcha #10, narrow perturbations leave too many cases passing
 * (e.g. only one sign + one exponent path); collapsing to a constant
 * guarantees every case fails the grader.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export interface StoreExponentResult {
  readonly bytes: bigint;
  readonly byte_length: number;
}

export function mpfr_fpif_store_exponent(x: MPFR): StoreExponentResult {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'bad input');
  }
  // BUG: collapse all encoding to a constant single-byte buffer.
  return { bytes: 0n, byte_length: 1 };
}
