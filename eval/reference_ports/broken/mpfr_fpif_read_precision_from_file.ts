/**
 * reference_ports/broken/mpfr_fpif_read_precision_from_file.ts -- deliberately-buggy.
 *
 * **BUG: collapses the entire decode to a constant
 * { precision: 1n, nextPos: pos + 1 } regardless of input.** Per HANDOFF
 * gotcha #10, narrow perturbations (off-by-one MPFR_MAX_PRECSIZE,
 * mis-shifted byte) leave many cases passing because the short-precision
 * single-byte path is the most common; collapsing to a constant
 * guarantees every case fails the grader.
 */

import { MPFRError } from '../../../src/core.ts';

export interface ReadPrecisionResult {
  readonly precision: bigint;
  readonly nextPos: number;
}

export function mpfr_fpif_read_precision_from_file(
  bytes_value: bigint,
  byte_length: number,
  pos: number,
): ReadPrecisionResult {
  if (typeof bytes_value !== 'bigint') {
    throw new MPFRError('EDOMAIN', 'bad input');
  }
  if (byte_length < 0 || !Number.isInteger(byte_length)) {
    throw new MPFRError('EDOMAIN', 'bad byte_length');
  }
  if (pos < 0 || !Number.isInteger(pos)) {
    throw new MPFRError('EDOMAIN', 'bad pos');
  }
  // BUG: constant output regardless of bytes.
  return { precision: 1n, nextPos: pos + 1 };
}
