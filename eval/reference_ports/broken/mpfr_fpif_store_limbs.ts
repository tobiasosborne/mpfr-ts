/**
 * reference_ports/broken/mpfr_fpif_store_limbs.ts -- deliberately-buggy.
 *
 * **BUG: collapses the entire encoding to a constant
 * { bytes: 0n, byte_length: 1 } regardless of input.** Per HANDOFF
 * gotcha #10, narrow perturbations (off-by-one pad_bits, wrong byte
 * order) leave too many cases passing because many test mantissas
 * happen to have leading or trailing zeros; collapsing to a constant
 * guarantees every case fails the grader.
 */

import type { MPFR } from '../../../src/core.ts';
import { MPFRError } from '../../../src/core.ts';

export interface StoreLimbsResult {
  readonly bytes: bigint;
  readonly byte_length: number;
}

export function mpfr_fpif_store_limbs(x: MPFR): StoreLimbsResult {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'bad input');
  }
  if (x.kind !== 'normal') {
    throw new MPFRError('EDOMAIN', 'kind must be normal');
  }
  // BUG: constant output, ignore all input.
  return { bytes: 0n, byte_length: 1 };
}
