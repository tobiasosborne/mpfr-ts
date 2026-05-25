/**
 * reference_ports/broken/mpfr_fpif_store_precision.ts -- deliberately-buggy.
 *
 * **BUG: collapses the entire encoding to a constant single-byte buffer
 * { bytes: 0n, byte_length: 1 } regardless of input.** Per HANDOFF
 * gotcha #10, narrow perturbations of the fpif encoding leave too many
 * cases passing (e.g. the single-byte embedded path is most common);
 * collapsing to a constant guarantees every case fails the grader.
 *
 * Composite expected well below 0.30.
 */

import { MPFRError } from '../../../src/core.ts';

export interface StorePrecisionResult {
  readonly bytes: bigint;
  readonly byte_length: number;
}

export function mpfr_fpif_store_precision(
  precision: bigint,
): StorePrecisionResult {
  if (typeof precision !== 'bigint') {
    throw new MPFRError('EDOMAIN', 'bad input');
  }
  if (precision < 1n) {
    throw new MPFRError('EDOMAIN', 'bad precision');
  }
  // BUG: collapse all encoding to a constant.
  return { bytes: 0n, byte_length: 1 };
}
